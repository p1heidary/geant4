//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications, and indicate your  *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//

#include "G4AgentLLMOpenAI.hh"

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

// --- minimal JSON: escape/serialize request, extract needed fields ---
// A full JSON parser would be a heavyweight dependency; the
// chat-completions response surface we consume is narrow (content string,
// tool_calls id/name/arguments, finish reason). Parsing is a small
// structural scanner, robust to whitespace and field order.

std::string JsonEscape(const std::string& in)
{
  std::string out;
  out.reserve(in.size() + 8);
  for (unsigned char c : in)
  {
    switch (c)
    {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20)
        {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        }
        else
        {
          out += char(c);
        }
    }
  }
  return out;
}

// Read a JSON string starting at s[i] == '"'; returns index past the
// closing quote, or npos. Unescapes standard sequences including \uXXXX.
std::size_t ReadString(const std::string& s, std::size_t i, std::string& out)
{
  if (i >= s.size() || s[i] != '\"') return std::string::npos;
  out.clear();
  ++i;
  while (i < s.size())
  {
    char c = s[i];
    if (c == '\"') return i + 1;
    if (c == '\\' && i + 1 < s.size())
    {
      char n = s[++i];
      switch (n)
      {
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case '\"':
          out += '\"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'u':
          if (i + 4 < s.size())
          {
            unsigned cp = (unsigned)std::stoul(s.substr(i + 1, 4), nullptr, 16);
            i += 4;
            if (cp < 0x80)
              out += char(cp);
            else if (cp < 0x800)
            {
              out += char(0xC0 | (cp >> 6));
              out += char(0x80 | (cp & 0x3F));
            }
            else
            {
              out += char(0xE0 | (cp >> 12));
              out += char(0x80 | ((cp >> 6) & 0x3F));
              out += char(0x80 | (cp & 0x3F));
            }
          }
          break;
        default:
          out += n;
      }
      ++i;
      continue;
    }
    out += c;
    ++i;
  }
  return std::string::npos;
}

std::size_t FindMatching(const std::string& s, std::size_t open, char l, char r)
{
  // Index past the bracket closing the one at s[open]; skips strings.
  if (open >= s.size() || s[open] != l) return std::string::npos;
  int depth = 0;
  bool inStr = false;
  for (std::size_t i = open; i < s.size(); ++i)
  {
    char c = s[i];
    if (inStr)
    {
      if (c == '\\')
        ++i;
      else if (c == '\"')
        inStr = false;
      continue;
    }
    if (c == '\"')
      inStr = true;
    else if (c == l)
      ++depth;
    else if (c == r)
    {
      --depth;
      if (depth == 0) return i + 1;
    }
  }
  return std::string::npos;
}

std::size_t SkipWs(const std::string& s, std::size_t i)
{
  while (i < s.size() && std::isspace((unsigned char)s[i]))
    ++i;
  return i;
}

bool FindMemberValue(const std::string& obj, const std::string& key, std::size_t& valueStart,
                     std::size_t& valueEnd)
{
  // Scan member positions of the object spanning [obj] (its braces may be
  // outside); finds the value of "key" at the top level of this object.
  std::size_t i = 0;
  while (i < obj.size())
  {
    if (obj[i] == '\"')
    {
      std::string str;
      std::size_t after = ReadString(obj, i, str);
      if (after == std::string::npos) return false;
      std::size_t colon = SkipWs(obj, after);
      if (colon < obj.size() && obj[colon] == ':')
      {
        std::size_t vs = SkipWs(obj, colon + 1);
        if (vs >= obj.size()) return false;
        char vc = obj[vs];
        std::size_t ve;
        if (vc == '{' || vc == '[')
        {
          char close = (vc == '{') ? '}' : ']';
          ve = FindMatching(obj, vs, vc, close);
        }
        else if (vc == '\"')
        {
          std::string dummy;
          ve = ReadString(obj, vs, dummy);
        }
        else
        {
          ve = vs;
          while (ve < obj.size() && obj[ve] != ',' && obj[ve] != '}' && obj[ve] != ']')
            ++ve;
        }
        if (ve == std::string::npos) return false;
        if (str == key)
        {
          valueStart = vs;
          valueEnd = ve;
          return true;
        }
        i = ve;
        continue;
      }
      i = after;
      continue;
    }
    ++i;
  }
  return false;
}

bool FindStringValue(const std::string& obj, const std::string& key, std::string& value)
{
  std::size_t vs, ve;
  if (!FindMemberValue(obj, key, vs, ve)) return false;
  if (vs >= obj.size() || obj[vs] != '\"') return false;
  return ReadString(obj, vs, value) != std::string::npos;
}

std::vector<std::string> SplitTopLevelArray(const std::string& arr)
{
  std::vector<std::string> out;
  std::size_t i = SkipWs(arr, 0);
  if (i >= arr.size() || arr[i] != '[') return out;
  i = SkipWs(arr, i + 1);
  if (i < arr.size() && arr[i] == ']') return out;
  while (i < arr.size())
  {
    std::size_t start = i;
    char c = arr[i];
    std::size_t end;
    if (c == '{')
      end = FindMatching(arr, i, '{', '}');
    else if (c == '[')
      end = FindMatching(arr, i, '[', ']');
    else if (c == '\"')
    {
      std::string d;
      end = ReadString(arr, i, d);
    }
    else
    {
      end = i;
      while (end < arr.size() && arr[end] != ',' && arr[end] != ']')
        ++end;
    }
    if (end == std::string::npos) break;
    out.push_back(arr.substr(start, end - start));
    i = SkipWs(arr, end);
    if (i < arr.size() && arr[i] == ',') i = SkipWs(arr, i + 1);
    if (i < arr.size() && arr[i] == ']') break;
  }
  return out;
}

std::size_t WriteCallback(void* contents, std::size_t size, std::size_t nmemb, void* userp)
{
  std::size_t bytes = size * nmemb;
  static_cast<std::string*>(userp)->append(static_cast<char*>(contents), bytes);
  return bytes;
}

}  // namespace

G4AgentLLMOpenAI::G4AgentLLMOpenAI(const G4AgentLLMConfig& config)
  : fUrl(config.BaseUrl() + "/chat/completions"),
    fApiKey(config.ApiKey()),
    fModel(config.Model()),
    fTimeoutSeconds(config.TimeoutSeconds())
{}

G4LLMReply G4AgentLLMOpenAI::Complete(const std::vector<G4LLMMessage>& messages,
                                      const std::vector<G4LLMToolSpec>& tools)
{
  G4LLMReply reply;

  // Build request body
  std::string body = "{\"model\":\"" + JsonEscape(fModel) + "\",\"messages\":[";
  bool first = true;
  for (const auto& m : messages)
  {
    if (!first) body += ",";
    first = false;
    const char* role = "user";
    if (m.fRole == G4LLMRole::System)
      role = "system";
    else if (m.fRole == G4LLMRole::Assistant)
      role = "assistant";
    else if (m.fRole == G4LLMRole::Tool)
      role = "tool";

    body += "{\"role\":\"";
    body += role;
    if (m.fRole == G4LLMRole::Assistant && !m.fToolCalls.empty())
    {
      // OpenAI shape: assistant message with tool_calls; content may be
      // null when the model requested tools.
      body += m.fContent.empty()
                ? "\",\"content\":null,\"tool_calls\":["
                : "\",\"content\":\"" + JsonEscape(std::string(m.fContent)) + "\",\"tool_calls\":[";
      for (std::size_t c = 0; c < m.fToolCalls.size(); ++c)
      {
        if (c > 0) body += ",";
        const auto& call = m.fToolCalls[c];
        body += "{\"id\":\"" + JsonEscape(std::string(call.fId))
                + "\",\"type\":\"function\",\"function\":{\"name\":\""
                + JsonEscape(std::string(call.fName)) + "\",\"arguments\":\""
                + JsonEscape(std::string(call.fArgument.empty()
                                           ? "{}"
                                           : "{\"argument\":\""
                                               + JsonEscape(std::string(call.fArgument)) + "\"}"))
                + "\"}}";
      }
      body += "]}";
      continue;
    }
    body += "\",\"content\":\"" + JsonEscape(std::string(m.fContent)) + "\"";
    if (m.fRole == G4LLMRole::Tool && !m.fToolCallId.empty())
      body += ",\"tool_call_id\":\"" + JsonEscape(std::string(m.fToolCallId)) + "\"";
    body += "}";
  }
  body += "]";

  if (!tools.empty())
  {
    body += ",\"tools\":[";
    for (std::size_t t = 0; t < tools.size(); ++t)
    {
      if (t > 0) body += ",";
      body += "{\"type\":\"function\",\"function\":{\"name\":\"";
      body += JsonEscape(std::string(tools[t].fName));
      body += "\",\"description\":\"";
      body += JsonEscape(std::string(tools[t].fDescription));
      body +=
        "\",\"parameters\":{\"type\":\"object\",\"properties\":{\"argument\":"
        "{\"type\":\"string\"}},\"required\":[]}}}";
    }
    body += "]";
  }
  body += "}";

  CURL* curl = curl_easy_init();
  if (curl == nullptr)
  {
    reply.fError = "curl initialization failed";
    return reply;
  }

  std::string response;
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  if (!fApiKey.empty())
  {
    std::string auth = "Authorization: Bearer " + std::string(fApiKey);
    headers = curl_slist_append(headers, auth.c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, fUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)fTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);

  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
  {
    reply.fError = G4String("connection failure: ") + curl_easy_strerror(res);
    return reply;
  }
  if (httpCode == 401 || httpCode == 403)
  {
    reply.fError = "authentication rejected (check G4_AGENT_API_KEY)";
    return reply;
  }
  if (httpCode < 200 || httpCode >= 300)
  {
    reply.fError = "HTTP error " + std::to_string(httpCode);
    return reply;
  }

  // Parse response: choices[0].message
  std::size_t vs, ve;
  if (!FindMemberValue(response, "choices", vs, ve))
  {
    reply.fError = "malformed response: no choices";
    return reply;
  }
  std::string choices = response.substr(vs, ve - vs);
  auto choiceElems = SplitTopLevelArray(choices);
  if (choiceElems.empty())
  {
    reply.fError = "malformed response: empty choices";
    return reply;
  }
  // first choice
  std::size_t ms, me;
  if (!FindMemberValue(choiceElems[0], "message", ms, me))
  {
    reply.fError = "malformed response: no message";
    return reply;
  }
  std::string message = choiceElems[0].substr(ms, me - ms);

  std::string content;
  FindStringValue(message, "content", content);
  reply.fText = content.c_str();

  std::string finish;
  if (FindStringValue(choiceElems[0], "finish_reason", finish))
    reply.fFinishReason = finish.c_str();

  if (FindMemberValue(message, "tool_calls", vs, ve))
  {
    std::string toolCalls = message.substr(vs, ve - vs);
    for (const auto& elem : SplitTopLevelArray(toolCalls))
    {
      std::size_t fs, fe;
      if (!FindMemberValue(elem, "function", fs, fe)) continue;
      std::string fn = elem.substr(fs, fe - fs);
      G4LLMToolCall call;
      std::string id;
      if (FindStringValue(elem, "id", id)) call.fId = id.c_str();
      std::string name;
      if (FindStringValue(fn, "name", name)) call.fName = name.c_str();
      std::string args;
      if (FindStringValue(fn, "arguments", args))
      {
        // arguments is a JSON-encoded string containing an object with our
        // single "argument" field; extract it.
        std::string argVal;
        if (FindStringValue(args, "argument", argVal)) call.fArgument = argVal.c_str();
      }
      reply.fToolCalls.push_back(call);
    }
  }

  reply.fOk = true;
  return reply;
}

// --- optional-module entry point resolved by the core at run time ---
extern "C" G4AgentLLMBackend* G4AgentLLMCreateHTTP(const G4AgentLLMConfig& config)
{
  return new G4AgentLLMOpenAI(config);
}
