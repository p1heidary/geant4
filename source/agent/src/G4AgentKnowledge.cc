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

#include "G4AgentKnowledge.hh"

#include "G4AgentReport.hh"
#include "G4Version.hh"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace
{
constexpr std::size_t kMaxDocs = 200;
constexpr std::size_t kHeaderScanLines = 20;

G4String Now()
{
  const std::time_t t = std::time(nullptr);
  char buf[64] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  return G4String(buf);
}

bool IsDocument(const G4String& name)
{
  auto ends = [&](const char* s) {
    const std::size_t n = std::strlen(s);
    return name.size() > n && name.compare(name.size() - n, n, s) == 0;
  };
  return ends(".md") || ends(".txt") || ends(".rst");
}

// Extract a "Key:" line value from the first lines of a document.
G4String Header(const G4String& block, const G4String& key)
{
  std::size_t start = 0;
  std::size_t counted = 0;
  while (start < block.size() && counted < kHeaderScanLines)
  {
    std::size_t nl = block.find('\n', start);
    if (nl == G4String::npos) nl = block.size();
    G4String line = block.substr(start, nl - start);
    start = nl + 1;
    ++counted;
    // ReadFile numbers lines as "  12: text"; strip the prefix.
    auto colon = line.find(": ");
    if (colon == G4String::npos) continue;
    G4String text = line.substr(colon + 2);
    if (text.rfind(key, 0) == 0)
    {
      auto v = text.find(':');
      if (v != G4String::npos)
      {
        G4String value = text.substr(v + 1);
        while (!value.empty() && value[0] == ' ')
          value = value.substr(1);
        return value;
      }
    }
  }
  return "";
}
}  // namespace

G4AgentKnowledge::G4AgentKnowledge(const G4String& corpusDir, const G4String& toolkitVersion)
  : fCorpus(corpusDir.empty() ? G4String("<unset>") : corpusDir),
    fToolkitVersion(toolkitVersion),
    fConfigured(!corpusDir.empty())
{}

G4String G4AgentKnowledge::ConfiguredCorpusDir(const G4String& projectRoot)
{
  const char* env = std::getenv("G4_AGENT_KNOWLEDGE_DIR");
  if (env != nullptr && *env != '\0') return G4String(env);
  if (projectRoot.empty()) return G4String("");
  return projectRoot + "/knowledge";
}

G4bool G4AgentKnowledge::Available() const
{
  return fConfigured && fCorpus.Usable();
}

G4String G4AgentKnowledge::Run(const G4String& subcommand, const G4String& argument) const
{
  G4String sub = subcommand;
  if (sub.empty()) sub = "status";

  if (sub == "status") return Status();
  if (sub == "catalog") return Catalog();
  if (sub == "search")
  {
    G4AgentReport report;
    report.SetHeader("[G4AGENT] knowledge search");
    if (!Available())
    {
      report.AddLine(
        "knowledge : NOT CONFIGURED (set G4_AGENT_KNOWLEDGE_DIR"
        " or create <project>/knowledge)");
      return report.AsString();
    }
    if (argument.empty())
    {
      report.AddLine("error : search term required");
      return report.AsString();
    }
    report.AddBlock(fCorpus.Search(argument, ""));
    return report.AsString();
  }
  if (sub == "gap") return RecordGap(argument);

  G4AgentReport report;
  report.SetHeader("[G4AGENT] knowledge");
  report.AddLine("error : unknown subcommand '" + sub + "' (status|catalog|search|gap)");
  return report.AsString();
}

G4String G4AgentKnowledge::Status() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] knowledge");
  if (!Available())
  {
    report.AddLine("knowledge : NOT CONFIGURED (normal; g4.txt 26)");
    report.AddLine(
      "hint      : set G4_AGENT_KNOWLEDGE_DIR or place docs under"
      " <project>/knowledge/");
    report.AddLine(
      "sources   : local plain-text corpus only; no network, no"
      " vector database, no bundled documentation");
    return report.AsString();
  }
  report.AddLine("knowledge : local corpus at " + fCorpus.Root());
  report.AddLine("toolkit   : " + fToolkitVersion);
  report.AddLine(
    "sources   : local plain-text corpus only; no network, no"
    " vector database, no bundled documentation");
  return report.AsString();
}

G4String G4AgentKnowledge::Catalog() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] knowledge catalog");
  if (!Available())
  {
    report.AddLine(
      "knowledge : NOT CONFIGURED (set G4_AGENT_KNOWLEDGE_DIR"
      " or create <project>/knowledge)");
    return report.AsString();
  }

  // Reuse the confined listing; entry lines are exactly "  <relpath>".
  const G4String listing = fCorpus.ListFiles("");
  std::vector<G4String> docs;
  std::size_t start = 0;
  while (start < listing.size() && docs.size() < kMaxDocs)
  {
    std::size_t nl = listing.find('\n', start);
    if (nl == G4String::npos) nl = listing.size();
    G4String line = listing.substr(start, nl - start);
    start = nl + 1;
    if (line.rfind("  ", 0) == 0 && IsDocument(line)) docs.push_back(line.substr(2));
  }
  std::sort(docs.begin(), docs.end());

  report.AddLine("documents : " + std::to_string(docs.size()));
  for (const auto& doc : docs)
  {
    const G4String body = fCorpus.ReadFile(doc);
    const G4String source = Header(body, "Source");
    const G4String version = Header(body, "Version");
    const G4String category = Header(body, "Category");
    G4String line = "  " + doc;
    if (!source.empty()) line += "  [source: " + source + "]";
    if (!category.empty()) line += "  [category: " + category + "]";
    if (!version.empty())
    {
      line +=
        "  [version: " + version
        + (fToolkitVersion.find(version) != G4String::npos ? " (matches toolkit)" : " (differs)")
        + "]";
    }
    report.AddLine(line);
  }
  report.AddLine(
    "note : version context is read from document headers; source"
    " authority is the operator's statement, not verified here");
  return report.AsString();
}

G4String G4AgentKnowledge::RecordGap(const G4String& note) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] knowledge gap");
  if (!Available())
  {
    report.AddLine("knowledge : NOT CONFIGURED; gap not recorded");
    return report.AsString();
  }
  G4String abs;
  if (!fCorpus.ResolveInside("GAPS.md", abs))
  {
    report.AddLine("error : cannot resolve GAPS.md inside the corpus");
    return report.AsString();
  }
  std::ofstream out(std::string{abs}, std::ios::app);
  if (!out)
  {
    report.AddLine("error : cannot write GAPS.md");
    return report.AsString();
  }
  const G4String text = note.empty() ? G4String("(unspecified)") : note;
  out << "\n## " << Now() << "\n- gap: " << text << "\n"
      << "- toolkit: " << fToolkitVersion << "\n";
  report.AddLine("recorded : " + text);
  report.AddLine("journal  : " + fCorpus.Root() + "/GAPS.md");
  return report.AsString();
}
