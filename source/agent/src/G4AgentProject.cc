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

#include "G4AgentProject.hh"

#include "G4AgentReport.hh"
#include "G4Filesystem.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#ifndef _WIN32
#  include <cstdio>
#endif

namespace
{
// G4String derives from std::string but does not satisfy the path source
// traits; always construct filesystem paths from an explicit std::string.
G4fs::path P(const G4String& x)
{
  return G4fs::path{std::string{x}};
}

constexpr std::size_t kMaxListEntries = 500;
constexpr std::size_t kMaxReadBytes = 256 * 1024;
constexpr std::size_t kMaxSearchFileBytes = 512 * 1024;
constexpr std::size_t kMaxSearchFiles = 5000;
constexpr std::size_t kMaxSearchMatches = 200;
constexpr std::size_t kMaxDiffBytes = 256 * 1024;
constexpr std::size_t kMaxWriteBytes = 1024 * 1024;
constexpr const char* kJournalName = "G4AGENT_CHANGES.md";

G4String NormalizeRoot(const G4String& requested)
{
  std::error_code ec;
  G4fs::path p = requested.empty() ? G4fs::current_path(ec) : P(requested);
  if (ec) return "";
  if (!p.is_absolute()) p = G4fs::absolute(p, ec);
  if (ec) return "";
  auto canon = G4fs::canonical(p, ec);
  if (!ec && !canon.empty()) return canon.string();
  return p.lexically_normal().string();
}

bool HasPrefix(const G4String& path, const G4String& root)
{
  if (path == root) return true;
  if (path.size() <= root.size() || path.compare(0, root.size(), root) != 0) return false;
  if (root == "/") return true;  // "/" already ends with the separator
  return path[root.size()] == '/';
}

bool IsBinary(std::istream& in)
{
  std::streampos pos = in.tellg();
  std::string buf;
  buf.resize(1024);
  in.read(&buf[0], 1024);
  auto n = static_cast<std::size_t>(in.gcount());
  buf.resize(n);
  in.clear();
  in.seekg(pos);
  return buf.find('\0') != std::string::npos;
}

G4String Relative(const G4String& abs, const G4String& root)
{
  if (!HasPrefix(abs, root)) return abs;
  if (abs == root) return ".";
  return abs.substr(root.size() + 1);
}

constexpr std::size_t kMaxProjectFileBytes = 512 * 1024;

G4String ReadSmallFile(const G4fs::path& path)
{
  std::error_code ec;
  if (!G4fs::is_regular_file(path, ec) || ec) return "";
  std::ifstream in(path.string(), std::ios::binary);
  if (!in) return "";
  std::string out;
  out.resize(kMaxProjectFileBytes);
  in.read(&out[0], static_cast<std::streamsize>(out.size()));
  out.resize(static_cast<std::size_t>(in.gcount()));
  return G4String(out);
}

bool IsRegularFile(const G4fs::path& path)
{
  std::error_code ec;
  return G4fs::is_regular_file(path, ec) && !ec;
}

G4fs::path NormalizePath(const G4String& text)
{
  std::error_code ec;
  auto p = P(text);
  auto canon = G4fs::canonical(p, ec);
  if (!ec) return canon;
  return p.lexically_normal();
}

bool SamePath(const G4String& left, const G4String& right)
{
  return NormalizePath(left) == NormalizePath(right);
}

G4String Lower(const G4String& text)
{
  G4String out = text;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

G4String Join(const std::vector<G4String>& values, const G4String& separator)
{
  G4String out;
  for (std::size_t i = 0; i < values.size(); ++i)
  {
    if (i != 0) out += separator;
    out += values[i];
  }
  return out;
}

bool SafeTargetName(const G4String& name)
{
  if (name.empty() || name == "." || name == "..") return false;
  for (unsigned char c : name)
  {
    if (!(std::isalnum(c) || c == '_' || c == '-' || c == '+' || c == '.')) return false;
  }
  return true;
}

// Remove CMake '#' comments while preserving newlines and quoted strings.
G4String StripCMakeComments(const G4String& text)
{
  G4String out;
  out.reserve(text.size());
  bool inQuote = false;
  for (std::size_t i = 0; i < text.size(); ++i)
  {
    const char c = text[i];
    if (c == '"' && (i == 0 || text[i - 1] != '\\')) inQuote = !inQuote;
    if (c == '#' && !inQuote)
    {
      while (i < text.size() && text[i] != '\n') ++i;
      out += '\n';
      continue;
    }
    out += c;
  }
  return out;
}

std::vector<G4String> CMakeTargets(const G4String& text)
{
  const G4String clean = StripCMakeComments(text);
  const G4String key = "add_executable";
  std::vector<G4String> targets;
  std::size_t pos = 0;
  while ((pos = clean.find(key, pos)) != G4String::npos)
  {
    const bool leftBoundary =
      pos == 0
      || !(std::isalnum(static_cast<unsigned char>(clean[pos - 1])) || clean[pos - 1] == '_');
    std::size_t i = pos + key.size();
    const bool rightBoundary =
      i >= clean.size()
      || !(std::isalnum(static_cast<unsigned char>(clean[i])) || clean[i] == '_');
    if (!leftBoundary || !rightBoundary)
    {
      pos = i;
      continue;
    }
    while (i < clean.size() && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
    if (i >= clean.size() || clean[i] != '(')
    {
      pos = i;
      continue;
    }
    ++i;
    while (i < clean.size() && std::isspace(static_cast<unsigned char>(clean[i]))) ++i;
    bool quoted = false;
    if (i < clean.size() && clean[i] == '"')
    {
      quoted = true;
      ++i;
    }
    const std::size_t start = i;
    while (i < clean.size())
    {
      const char c = clean[i];
      if (quoted)
      {
        if (c == '"') break;
      }
      else if (std::isspace(static_cast<unsigned char>(c)) || c == ')')
      {
        break;
      }
      ++i;
    }
    const G4String name = clean.substr(start, i - start);
    if (SafeTargetName(name)) targets.push_back(name);
    pos = i;
  }
  std::sort(targets.begin(), targets.end());
  targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
  return targets;
}

G4String MakeTarget(const G4String& text)
{
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line))
  {
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size() || line[i] == '#') continue;
    if (line.compare(i, 4, "name") != 0) continue;
    i += 4;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) continue;
    if (line[i] == ':')
    {
      ++i;
      if (i >= line.size() || line[i] != '=') continue;
      ++i;
    }
    else if (line[i] == '?' || line[i] == '+')
    {
      ++i;
      if (i >= line.size() || line[i] != '=') continue;
      ++i;
    }
    else if (line[i] == '=')
    {
      ++i;
    }
    else
    {
      continue;
    }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    const std::size_t start = i;
    while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))
           && line[i] != '#')
    {
      ++i;
    }
    const G4String name = line.substr(start, i - start);
    return SafeTargetName(name) ? name : G4String("");
  }
  return "";
}

G4String CacheHome(const G4fs::path& cachePath)
{
  std::istringstream lines(ReadSmallFile(cachePath));
  std::string line;
  const std::string key = "CMAKE_HOME_DIRECTORY:";
  while (std::getline(lines, line))
  {
    if (line.compare(0, key.size(), key) != 0) continue;
    const auto eq = line.find('=');
    if (eq == std::string::npos) return "";
    G4String value = line.substr(eq + 1);
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n')) value.pop_back();
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
      value = value.substr(1, value.size() - 2);
    return value;
  }
  return "";
}

enum class CacheState
{
  None,
  Valid,
  Foreign,
  Unusable
};

bool ConfinedRegular(const G4AgentProject& project, const G4String& relative)
{
  G4String abs;
  return project.ResolveInside(relative, abs) && IsRegularFile(P(abs));
}

CacheState ConfinedCacheState(const G4AgentProject& project, const G4String& relative)
{
  std::error_code ec;
  const G4fs::path path = P(project.Root()) / P(relative);
  if (!G4fs::exists(path, ec) || ec) return CacheState::None;
  G4String abs;
  if (!project.ResolveInside(relative, abs)) return CacheState::Foreign;
  if (!IsRegularFile(P(abs))) return CacheState::None;
  const G4String home = CacheHome(P(abs));
  if (home.empty()) return CacheState::Unusable;
  return SamePath(home, project.Root()) ? CacheState::Valid : CacheState::Foreign;
}

struct ProjectLayout
{
  CacheState outCache = CacheState::None;
  CacheState inCache = CacheState::None;
  G4bool hasCMakeLists = false;
  G4bool hasMakefile = false;
  G4bool buildEscapes = false;
};

ProjectLayout InspectLayout(const G4AgentProject& project)
{
  ProjectLayout layout;
  layout.outCache = ConfinedCacheState(project, "build/CMakeCache.txt");
  layout.inCache = ConfinedCacheState(project, "CMakeCache.txt");
  layout.hasCMakeLists = ConfinedRegular(project, "CMakeLists.txt");
  layout.hasMakefile = ConfinedRegular(project, "GNUmakefile");
  std::error_code ec;
  const G4fs::path build = P(project.Root()) / "build";
  if (G4fs::exists(build, ec) && !ec)
  {
    G4String abs;
    layout.buildEscapes = !project.ResolveInside("build", abs);
  }
  return layout;
}
}  // namespace

G4String G4AgentProject::ConfiguredRoot()
{
  const char* env = std::getenv("G4_AGENT_PROJECT_DIR");
  return env != nullptr ? G4String(env) : G4String("");
}

G4String G4AgentProject::ConfiguredBuildCommand()
{
  const char* env = std::getenv("G4_AGENT_BUILD_COMMAND");
  return env != nullptr ? G4String(env) : G4String("");
}

G4String G4AgentProject::ConfiguredRunCommand()
{
  const char* env = std::getenv("G4_AGENT_RUN_COMMAND");
  return env != nullptr ? G4String(env) : G4String("");
}

G4AgentProject::Command G4AgentProject::EffectiveBuildCommand() const
{
  Command out;
  const G4String configured = ConfiguredBuildCommand();
  if (!configured.empty())
  {
    out.text = configured;
    out.source = "environment (G4_AGENT_BUILD_COMMAND)";
    return out;
  }
  if (!fUsable)
  {
    out.source = "unconfigured";
    out.reason = "project root unavailable";
    return out;
  }

  const ProjectLayout layout = InspectLayout(*this);

  if (layout.outCache == CacheState::Valid)
  {
    out.text = "cmake --build build --parallel";
    out.source = "detected (CMake build tree 'build')";
    return out;
  }
  if (layout.inCache == CacheState::Valid)
  {
    out.text = "cmake --build . --parallel";
    out.source = "detected (CMake in-source build)";
    return out;
  }
  if (layout.hasCMakeLists)
  {
    if (layout.buildEscapes)
    {
      out.source = "unconfigured";
      out.reason = "build/ resolves outside the project root; set G4_AGENT_BUILD_COMMAND";
      return out;
    }
    if (layout.outCache == CacheState::Foreign || layout.outCache == CacheState::Unusable)
    {
      out.source = "unconfigured";
      out.reason = "build/CMakeCache.txt belongs to a different source tree or is "
                   "unreadable; set G4_AGENT_BUILD_COMMAND";
      return out;
    }
    out.text = "cmake -S . -B build && cmake --build build --parallel";
    out.source = "detected (CMake configure+build)";
    return out;
  }
  if (layout.hasMakefile)
  {
    out.text = "make -j";
    out.source = "detected (GNUmakefile)";
    return out;
  }
  out.source = "unconfigured";
  out.reason = "no supported build system detected (expected CMakeLists.txt or GNUmakefile)";
  return out;
}

G4AgentProject::Command G4AgentProject::EffectiveRunCommand() const
{
  Command out;
  const G4String configured = ConfiguredRunCommand();
  if (!configured.empty())
  {
    out.text = configured;
    out.source = "environment (G4_AGENT_RUN_COMMAND)";
    return out;
  }
  if (!fUsable)
  {
    out.source = "unconfigured";
    out.reason = "project root unavailable";
    return out;
  }

  const G4fs::path root = P(fRoot);
  const ProjectLayout layout = InspectLayout(*this);
  const bool cmakeProject = layout.hasCMakeLists || layout.outCache == CacheState::Valid
                            || layout.inCache == CacheState::Valid;

  G4String executable;
  G4String source;
  G4String prefix;

  if (cmakeProject)
  {
    if (!layout.hasCMakeLists)
    {
      out.source = "unconfigured";
      out.reason = "CMake build tree found but CMakeLists.txt is missing; "
                   "set G4_AGENT_RUN_COMMAND";
      return out;
    }
    const std::vector<G4String> targets = CMakeTargets(ReadSmallFile(root / "CMakeLists.txt"));
    if (targets.empty())
    {
      out.source = "unconfigured";
      out.reason = "no literal add_executable target found in CMakeLists.txt; "
                   "set G4_AGENT_RUN_COMMAND";
      return out;
    }
    if (targets.size() == 1)
    {
      executable = targets.front();
      source = "detected (CMake target '" + executable + "')";
    }
    else
    {
      const G4String directory = root.filename().string();
      std::vector<G4String> matches;
      for (const auto& target : targets)
      {
        if (!directory.empty() && target == directory) matches.push_back(target);
      }
      if (matches.empty())
      {
        const G4String lowerDirectory = Lower(directory);
        for (const auto& target : targets)
        {
          if (!lowerDirectory.empty() && Lower(target) == lowerDirectory)
            matches.push_back(target);
        }
      }
      if (matches.size() != 1)
      {
        out.source = "unconfigured";
        out.reason = "multiple executable targets found (" + Join(targets, ", ")
                     + "); set G4_AGENT_RUN_COMMAND";
        return out;
      }
      executable = matches.front();
      source = "detected (CMake target '" + executable + "', directory match)";
    }
    if (layout.outCache == CacheState::Valid)
    {
      prefix = "./build/";
    }
    else if (layout.inCache == CacheState::Valid)
    {
      prefix = "./";
    }
    else if (layout.buildEscapes)
    {
      out.source = "unconfigured";
      out.reason = "build/ resolves outside the project root; set G4_AGENT_RUN_COMMAND";
      return out;
    }
    else if (layout.outCache == CacheState::Foreign || layout.outCache == CacheState::Unusable)
    {
      out.source = "unconfigured";
      out.reason = "build/CMakeCache.txt belongs to a different source tree or is "
                   "unreadable; set G4_AGENT_RUN_COMMAND";
      return out;
    }
    else
    {
      prefix = "./build/";  // /build configures it on first use
    }
  }
  else if (layout.hasMakefile)
  {
    executable = MakeTarget(ReadSmallFile(root / "GNUmakefile"));
    if (executable.empty())
    {
      out.source = "unconfigured";
      out.reason = "no usable 'name :=' target found in GNUmakefile; "
                   "set G4_AGENT_RUN_COMMAND";
      return out;
    }
    source = "detected (GNUmakefile target '" + executable + "')";
    prefix = "./";
  }
  else
  {
    out.source = "unconfigured";
    out.reason = "no supported build system detected (expected CMakeLists.txt or GNUmakefile)";
    return out;
  }

  out.text = prefix + executable;
  out.source = source;
  return out;
}

G4String G4AgentProject::SanitizeForShell(const G4String& text)
{
  G4String out;
  for (char c : text)
  {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  return out;
}

G4AgentProject G4AgentProject::FromEnvironment()
{
  const char* env = std::getenv("G4_AGENT_PROJECT_DIR");
  return G4AgentProject(NormalizeRoot(env != nullptr ? G4String(env) : G4String("")));
}

G4AgentProject::G4AgentProject(const G4String& rootPath) : fRoot(NormalizeRoot(rootPath))
{
  std::error_code ec;
  fUsable = !fRoot.empty() && G4fs::is_directory(P(fRoot), ec) && !ec;
}

G4bool G4AgentProject::ResolveInside(const G4String& path, G4String& resolved) const
{
  resolved.clear();
  if (!fUsable) return false;

  if (path.empty())
  {
    resolved = fRoot;
    return true;
  }

  auto rel = P(path);
  if (rel.is_absolute()) return false;  // project-relative arguments only

  auto joined = (P(fRoot) / rel).lexically_normal();
  G4String js = joined.string();
  if (!HasPrefix(js, fRoot)) return false;  // '..' escape (lexical)

  // Symlink escape check once the target exists.
  std::error_code ec;
  auto canon = G4fs::canonical(joined, ec);
  if (!ec && !canon.empty())
  {
    // The root itself may be reached through a symlink; compare against a
    // canonicalized root.
    auto canonRoot = G4fs::canonical(P(fRoot), ec);
    G4String cs = canon.string();
    if (!ec && !HasPrefix(cs, canonRoot.string())) return false;
    if (!ec) resolved = cs;
  }
  if (resolved.empty()) resolved = js;
  return true;
}

G4String G4AgentProject::WriteFile(const G4String& path, const G4String& content) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project write");
  if (path.empty())
  {
    report.AddLine("error : path required");
    return report.AsString();
  }
  G4String abs;
  if (!ResolveInside(path, abs))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }
  if (content.find('\0') != G4String::npos)
  {
    report.AddLine("error : binary content refused");
    return report.AsString();
  }
  G4String rel = Relative(abs, fRoot);
  if (rel == kJournalName)
  {
    report.AddLine("error : the journal is agent-owned and immutable");
    return report.AsString();
  }
  if (content.size() > kMaxWriteBytes)
  {
    report.AddLine("error : content exceeds " + std::to_string(kMaxWriteBytes) + " bytes");
    return report.AsString();
  }

  std::error_code ec;
  G4fs::create_directories(G4fs::path(std::string(abs)).parent_path(), ec);
  std::ofstream out(std::string{abs}, std::ios::binary | std::ios::trunc);
  if (!out)
  {
    report.AddLine("error : cannot write '" + rel + "'");
    return report.AsString();
  }
  out << content;
  out.close();
  if (!out)
  {
    report.AddLine("error : write to '" + rel + "' failed");
    return report.AsString();
  }
  report.AddLine("file  : " + rel);
  report.AddLine("bytes : " + std::to_string(content.size()));
  report.AddLine("note  : change journal-recorded when run via the ask loop");
  return report.AsString();
}

void G4AgentProject::JournalAppend(const G4String& block) const
{
  if (!fUsable) return;
  std::ofstream out(std::string{G4fs::path(std::string(fRoot)) / kJournalName}, std::ios::app);
  if (!out) return;
  out << block;
}

G4String G4AgentProject::ListFiles(const G4String& directory) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project list");
  if (!fUsable)
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }
  G4String base;
  if (!ResolveInside(directory, base))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }

  std::error_code ec;
  if (!G4fs::is_directory(P(base), ec) || ec)
  {
    report.AddLine("error : '" + directory + "' is not a directory");
    return report.AsString();
  }

  std::vector<G4String> entries;
  G4fs::recursive_directory_iterator it(P(base), G4fs::directory_options::skip_permission_denied,
                                        ec);
  G4fs::recursive_directory_iterator end;
  if (ec)
  {
    report.AddLine("error : cannot open directory");
    return report.AsString();
  }
  bool truncated = false;
  for (; !ec && it != end; it.increment(ec))
  {
    if (entries.size() >= kMaxListEntries)
    {
      truncated = true;
      break;
    }
    auto depth = it.depth();
    if (depth > 12)
    {
      it.disable_recursion_pending();
      continue;
    }
    G4String name = it->path().filename().string();
    if (name == ".git" || name == ".svn")
    {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec))
      entries.push_back(Relative(it->path().string(), fRoot) + "/");
    else
      entries.push_back(Relative(it->path().string(), fRoot));
  }

  std::sort(entries.begin(), entries.end());
  report.AddLine("directory : '" + (directory.empty() ? "." : directory) + "'");
  report.AddLine("count : " + std::to_string(entries.size()));
  for (const auto& e : entries)
    report.AddLine("  " + e);
  if (truncated)
    report.AddLine("note : listing truncated at " + std::to_string(kMaxListEntries) + " entries");
  return report.AsString();
}

G4String G4AgentProject::ReadFile(const G4String& path) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project read");
  if (path.empty())
  {
    report.AddLine("error : path required");
    return report.AsString();
  }
  G4String abs;
  if (!ResolveInside(path, abs))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }

  std::error_code ec;
  if (!G4fs::is_regular_file(P(abs), ec) || ec)
  {
    report.AddLine("error : '" + path + "' is not a regular file");
    return report.AsString();
  }

  std::ifstream in(std::string{abs}, std::ios::binary);
  if (!in)
  {
    report.AddLine("error : cannot open '" + path + "'");
    return report.AsString();
  }
  if (IsBinary(in))
  {
    report.AddLine("file : " + Relative(abs, fRoot));
    report.AddLine("note : binary file (not shown)");
    return report.AsString();
  }

  report.AddLine("file : " + Relative(abs, fRoot));
  std::string line;
  std::size_t n = 0;
  std::size_t bytes = 0;
  bool truncated = false;
  while (std::getline(in, line))
  {
    ++n;
    if (bytes + line.size() > kMaxReadBytes)
    {
      truncated = true;
      break;
    }
    bytes += line.size() + 1;
    std::ostringstream os;
    os << n << ": " << line;
    report.AddLine(os.str());
  }
  if (truncated)
    report.AddLine("note : truncated at " + std::to_string(kMaxReadBytes) + " bytes ("
                   + std::to_string(n) + " lines shown)");
  return report.AsString();
}

G4String G4AgentProject::Search(const G4String& pattern, const G4String& directory) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project search");
  if (pattern.empty())
  {
    report.AddLine("error : pattern required");
    return report.AsString();
  }
  G4String base;
  if (!ResolveInside(directory, base))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }

  std::vector<G4String> files;
  std::error_code ec;
  if (G4fs::is_regular_file(P(base), ec))
  {
    files.push_back(base);
  }
  else if (G4fs::is_directory(P(base), ec))
  {
    G4fs::recursive_directory_iterator it(P(base), G4fs::directory_options::skip_permission_denied,
                                          ec);
    G4fs::recursive_directory_iterator end;
    for (; !ec && it != end && files.size() < kMaxSearchFiles; it.increment(ec))
    {
      if (it.depth() > 12)
      {
        it.disable_recursion_pending();
        continue;
      }
      G4String name = it->path().filename().string();
      if (name == ".git" || name == ".svn")
      {
        if (it->is_directory(ec)) it.disable_recursion_pending();
        continue;
      }
      if (it->is_regular_file(ec)) files.push_back(it->path().string());
    }
  }
  else
  {
    report.AddLine("error : '" + directory + "' not found");
    return report.AsString();
  }

  std::sort(files.begin(), files.end());
  std::size_t matches = 0;
  report.AddLine("pattern : " + pattern);
  for (const auto& f : files)
  {
    if (matches >= kMaxSearchMatches) break;
    std::ifstream in(std::string{f}, std::ios::binary);
    if (!in) continue;
    std::size_t size = 0;
    std::string line;
    std::size_t ln = 0;
    while (std::getline(in, line) && size < kMaxSearchFileBytes)
    {
      ++ln;
      size += line.size() + 1;
      if (line.find('\0') != std::string::npos) break;  // binary: stop file
      auto pos = line.find(pattern.c_str());
      if (pos != std::string::npos)
      {
        std::ostringstream os;
        os << Relative(f, fRoot) << ":" << ln << ": " << line.substr(0, 200);
        report.AddLine(os.str());
        ++matches;
        if (matches >= kMaxSearchMatches) break;
      }
    }
  }
  report.AddLine("matches : " + std::to_string(matches)
                 + (matches >= kMaxSearchMatches ? " (truncated)" : ""));
  return report.AsString();
}

G4String G4AgentProject::Diff(const G4String& target) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project diff");
  if (!fUsable)
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }
  G4String abs;
  if (!ResolveInside(target, abs))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }
  G4String rel = Relative(abs, fRoot);

#ifndef _WIN32
  // Fixed read-only git invocation: clean environment, escaped confined
  // pathspec as a literal argument (no model-controlled command text).
  auto quote = [](const G4String& s) {
    G4String out = "'";
    for (char c : s)
    {
      if (c == '\'')
        out += "'\\''";
      else
        out += c;
    }
    out += "'";
    return out;
  };
  G4String cmd = "env -i LC_ALL=C git -C " + quote(fRoot) + " --no-pager diff --no-ext-diff -- "
                 + quote(rel.empty() ? "." : rel) + " 2>&1";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr)
  {
    report.AddLine("error : git unavailable");
    return report.AsString();
  }
  std::string out;
  char buf[4096];
  std::size_t n = 0;
  bool truncated = false;
  while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
  {
    if (out.size() + n > kMaxDiffBytes)
    {
      truncated = true;
      break;
    }
    out.append(buf, n);
  }
  int status = pclose(pipe);
  report.AddLine("target : " + (rel.empty() ? "." : rel));
  if (status != 0)
  {
    // git unavailable / not a checkout: report the state, never raw stderr.
    report.AddLine("note : no git baseline available (status " + std::to_string(status) + ")");
    return report.AsString();
  }
  if (out.empty())
  {
    report.AddLine("note : no local changes");
    return report.AsString();
  }
  std::istringstream lines(out);
  std::string line;
  while (std::getline(lines, line))
    report.AddLine(line);
  if (truncated)
    report.AddLine("note : diff truncated at " + std::to_string(kMaxDiffBytes) + " bytes");
#else
  report.AddLine("note : diff view requires a POSIX platform in this build");
#endif
  return report.AsString();
}
