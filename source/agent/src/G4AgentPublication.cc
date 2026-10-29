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
// *                                                                  *
// ********************************************************************
//

#include "G4AgentPublication.hh"

#include "G4AgentAnalysis.hh"
#include "G4AgentReport.hh"
#include "G4Filesystem.hh"
#include "G4Version.hh"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace
{
G4fs::path P(const G4String& x)
{
  return G4fs::path{std::string{x}};
}

constexpr std::size_t kMaxFileBytes = 2 * 1024 * 1024;

G4String Trim(const G4String& s)
{
  std::size_t a = 0;
  std::size_t b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])) != 0)
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])) != 0)
    --b;
  return s.substr(a, b - a);
}

G4String ToLower(G4String s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool ReadWholeFile(const G4String& absPath, std::string& out)
{
  std::ifstream in(std::string{absPath}, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  if (out.size() > kMaxFileBytes) out.resize(kMaxFileBytes);
  return true;
}

// Parse all `key : value` lines of a text into a map (keys keep internal
// spaces, e.g. "exit status"). Blank lines and comments are ignored.
std::map<G4String, G4String> ParseFields(const std::string& text)
{
  std::map<G4String, G4String> fields;
  std::istringstream in(text);
  std::string raw;
  while (std::getline(in, raw))
  {
    const G4String line = Trim(G4String(raw));
    if (line.empty() || line[0] == '#') continue;
    const auto colon = line.find(':');
    if (colon == G4String::npos) continue;
    const G4String key = ToLower(Trim(line.substr(0, colon)));
    const G4String value = Trim(line.substr(colon + 1));
    if (!key.empty()) fields[key] = value;
  }
  return fields;
}

G4String Field(const std::map<G4String, G4String>& f, const G4String& key)
{
  auto it = f.find(ToLower(key));
  return it != f.end() ? it->second : G4String("");
}

G4String FirstErrorLine(const G4String& text)
{
  std::istringstream in(std::string{text});
  std::string line;
  while (std::getline(in, line))
  {
    if (line.rfind("error", 0) == 0)
    {
      const auto colon = line.find(':');
      return Trim(G4String(colon == std::string::npos ? line : line.substr(colon + 1)));
    }
  }
  return "no comparison produced";
}

G4String Truncate(G4String s, std::size_t n)
{
  if (s.size() > n) s = s.substr(0, n - 3) + "...";
  return s;
}
}  // namespace

G4String G4AgentPublication::ConfiguredReferencesDir(const G4String& projectRoot)
{
  const char* env = std::getenv("G4_AGENT_REFERENCES_DIR");
  if (env != nullptr && *env != '\0') return G4String(env);
  return projectRoot.empty() ? G4String("references") : projectRoot + G4String("/references");
}

G4AgentPublication::G4AgentPublication(const G4AgentProject& project,
                                       const G4AgentAnalysis& analysis)
  : fProject(project), fAnalysis(analysis), fReferences(ConfiguredReferencesDir(project.Root()))
{}

G4String G4AgentPublication::Citation(const Reference& ref)
{
  G4String cite;
  if (!ref.authors.empty()) cite += ref.authors;
  if (!ref.year.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += "(" + ref.year + ").";
  }
  if (!ref.title.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += ref.title + ".";
  }
  if (!ref.venue.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += ref.venue + ".";
  }
  if (!ref.doi.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += "doi:" + ref.doi + ".";
  }
  if (!ref.arxiv.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += "arXiv:" + ref.arxiv + ".";
  }
  if (!ref.url.empty())
  {
    if (!cite.empty()) cite += " ";
    cite += ref.url;
  }
  if (cite.empty()) cite = ref.id + " (metadata not available)";
  return cite;
}

G4bool G4AgentPublication::LoadReferences(std::vector<Reference>& out, G4String& reason) const
{
  out.clear();
  if (!fReferences.Usable())
  {
    reason = "references corpus not configured";
    return false;
  }
  std::error_code ec;
  std::vector<G4String> files;
  for (const auto& entry : G4fs::directory_iterator(P(fReferences.Root()), ec))
  {
    if (!entry.is_regular_file(ec)) continue;
    const std::string name = entry.path().filename().string();
    if (name.size() > 4 && name.substr(name.size() - 4) == ".txt") files.push_back(name);
  }
  std::sort(files.begin(), files.end());

  for (const auto& name : files)
  {
    std::string text;
    const G4String abs = (P(fReferences.Root()) / std::string{name}).string();
    if (!ReadWholeFile(abs, text)) continue;
    const auto fields = ParseFields(text);
    Reference ref;
    ref.id = name.substr(0, name.size() - 4);
    const G4String declared = Field(fields, "id");
    if (!declared.empty()) ref.id = declared;
    ref.title = Field(fields, "title");
    ref.authors = Field(fields, "authors");
    ref.year = Field(fields, "year");
    ref.venue = Field(fields, "venue");
    ref.doi = Field(fields, "doi");
    ref.arxiv = Field(fields, "arxiv");
    ref.url = Field(fields, "url");
    ref.accessed = Field(fields, "accessed");
    ref.note = Field(fields, "note");
    out.push_back(ref);
  }
  if (out.empty())
  {
    reason = "no reference records in the corpus";
    return false;
  }
  return true;
}

G4String G4AgentPublication::ReadOptional(const G4String& relPath) const
{
  G4String abs;
  if (!fProject.Usable() || !fProject.ResolveInside(relPath, abs)) return "";
  std::error_code ec;
  if (!G4fs::is_regular_file(P(abs), ec) || ec) return "";
  std::string text;
  if (!ReadWholeFile(abs, text)) return "";
  return Trim(G4String(text));
}

G4String G4AgentPublication::Bibliography(const G4String& subcommand,
                                          const G4String& argument) const
{
  const G4String sub = Trim(subcommand);
  const G4String arg = Trim(argument);
  G4AgentReport report;

  std::vector<Reference> refs;
  G4String reason;
  const G4bool have = LoadReferences(refs, reason);

  if (sub.empty() || sub == "status")
  {
    report.SetHeader("[G4AGENT] references status");
    report.AddLine("corpus : "
                   + (fReferences.Usable() ? fReferences.Root()
                                           : G4String("NOT CONFIGURED (set "
                                                      "G4_AGENT_REFERENCES_DIR or create "
                                                      "references/)")));
    report.AddLine("count  : " + (have ? std::to_string(refs.size()) : G4String("0")));
    if (!have) report.AddLine("note   : " + reason);
    report.AddLine(
      "note   : sources are operator-supplied; the agent never invents a "
      "citation and never fetches the network in the core");
    return report.AsString();
  }
  if (sub == "list")
  {
    report.SetHeader("[G4AGENT] references list");
    if (!have)
    {
      report.AddLine("error : " + reason);
      return report.AsString();
    }
    report.AddLine("count : " + std::to_string(refs.size()));
    for (const auto& r : refs)
    {
      G4String ident =
        r.doi.empty() ? (r.arxiv.empty() ? G4String("") : "arXiv:" + r.arxiv) : "doi:" + r.doi;
      report.AddLine("  [" + r.id + "] " + r.year + "  " + Truncate(r.authors, 32) + "  "
                     + Truncate(r.title, 48) + "  " + ident);
    }
    return report.AsString();
  }
  if (sub == "show" || sub == "cite")
  {
    if (arg.empty())
    {
      report.SetHeader("[G4AGENT] references " + sub);
      report.AddLine("error : reference id required");
      return report.AsString();
    }
    if (!have)
    {
      report.SetHeader("[G4AGENT] references " + sub);
      report.AddLine("error : " + reason);
      return report.AsString();
    }
    for (const auto& r : refs)
    {
      if (r.id != arg) continue;
      if (sub == "cite")
      {
        report.SetHeader("[G4AGENT] reference cite");
        report.AddLine("id      : " + r.id);
        report.AddLine("citation: " + Citation(r));
        report.AddLine(
          "note    : assembled from declared fields only; missing fields are "
          "omitted, never invented");
        return report.AsString();
      }
      report.SetHeader("[G4AGENT] reference show");
      report.AddLine("id       : " + r.id);
      report.AddLine("title    : " + (r.title.empty() ? G4String("NOT AVAILABLE") : r.title));
      report.AddLine("authors  : " + (r.authors.empty() ? G4String("NOT AVAILABLE") : r.authors));
      report.AddLine("year     : " + (r.year.empty() ? G4String("NOT AVAILABLE") : r.year));
      report.AddLine("venue    : " + (r.venue.empty() ? G4String("NOT AVAILABLE") : r.venue));
      report.AddLine("doi      : " + (r.doi.empty() ? G4String("NOT AVAILABLE") : r.doi));
      report.AddLine("arxiv    : " + (r.arxiv.empty() ? G4String("NOT AVAILABLE") : r.arxiv));
      report.AddLine("url      : " + (r.url.empty() ? G4String("NOT AVAILABLE") : r.url));
      report.AddLine("accessed : " + (r.accessed.empty() ? G4String("NOT AVAILABLE") : r.accessed));
      report.AddLine("note     : " + (r.note.empty() ? G4String("NOT AVAILABLE") : r.note));
      return report.AsString();
    }
    report.SetHeader("[G4AGENT] references " + sub);
    report.AddLine("error : reference '" + arg + "' not found");
    return report.AsString();
  }

  report.SetHeader("[G4AGENT] references");
  report.AddLine("error : unknown subcommand '" + subcommand + "'");
  report.AddLine("usage : status | list | show <id> | cite <id>");
  return report.AsString();
}

G4String G4AgentPublication::Manuscript() const
{
  std::ostringstream md;
  G4String title = ReadOptional("research/title.txt");
  if (title.empty())
  {
    title = "G4Agent study: " + G4String(P(fProject.Root()).filename().string());
  }
  md << "# " << title << "\n\n";
  md << "> Provenance tags: [operator] [assumption] [simulated] [derived] "
        "[literature] [experimental]. Every section is tagged; absent inputs are "
        "reported NOT AVAILABLE and are never invented.\n\n";

  auto section = [&](const char* heading, const char* tag, const G4String& body,
                     const char* missing) {
    md << "## " << heading << " [" << tag << "]\n\n";
    if (body.empty())
      md << missing << "\n\n";
    else
    {
      md << body;
      if (body.back() != '\n') md << "\n";
      md << "\n";
    }
  };

  section("Objective", "operator", ReadOptional("research/problem.md"),
          "NOT AVAILABLE (no research/problem.md)");
  section("Assumptions and limitations", "assumption", ReadOptional("research/assumptions.md"),
          "NOT AVAILABLE (no research/assumptions.md)");
  section("Method", "operator", ReadOptional("research/design.md"),
          "NOT AVAILABLE (no research/design.md)");

  // --- simulated runs (verbatim from manifests) ---------------------------
  md << "## Simulation runs [simulated]\n\n";
  std::vector<G4String> runIds;
  {
    std::error_code ec;
    const G4fs::path runsRoot = P(fProject.Root()) / "runs";
    if (G4fs::is_directory(runsRoot, ec) && !ec)
    {
      for (const auto& entry : G4fs::directory_iterator(runsRoot, ec))
      {
        if (!entry.is_directory(ec)) continue;
        const std::string name = entry.path().filename().string();
        if (!name.empty() && std::find_if(name.begin(), name.end(), [](unsigned char c) {
                               return std::isdigit(c) == 0;
                             }) == name.end())
          runIds.push_back(name);
      }
    }
    std::sort(runIds.begin(), runIds.end());
  }
  if (runIds.empty())
  {
    md << "NOT AVAILABLE (no recorded runs)\n\n";
  }
  else
  {
    for (const auto& id : runIds)
    {
      std::string text;
      G4String manifestAbs;
      if (!fProject.ResolveInside("runs/" + id + "/manifest.txt", manifestAbs)
          || !ReadWholeFile(manifestAbs, text))
        continue;
      const auto fields = ParseFields(text);
      md << "- run " << id << ": macro=" << Field(fields, "macro")
         << ", exit status=" << Field(fields, "exit status")
         << ", duration=" << Field(fields, "duration")
         << ", source revision=" << Field(fields, "source revision") << "\n";
    }
    md << "\n";
  }

  // --- derived comparison -------------------------------------------------
  md << "## Results and comparison [derived]\n\n";
  if (runIds.empty())
  {
    md << "INSUFFICIENT EVIDENCE (no run to compare)\n\n";
  }
  else
  {
    const G4String comparison = fAnalysis.Run("compare", runIds.back());
    if (comparison.find("error") != G4String::npos)
      md << "INSUFFICIENT EVIDENCE (" << FirstErrorLine(comparison) << ")\n\n";
    else
    {
      md << "Computed by the harness (Stage-F pull test); not scientific acceptance.\n\n";
      md << "```\n" << comparison << "\n```\n\n";
    }
  }

  // --- literature ---------------------------------------------------------
  md << "## References [literature]\n\n";
  std::vector<Reference> refs;
  G4String reason;
  if (!LoadReferences(refs, reason))
  {
    md << "NOT AVAILABLE (" << reason << ")\n\n";
  }
  else
  {
    int n = 0;
    for (const auto& r : refs)
    {
      ++n;
      md << n << ". [" << r.id << "] " << Citation(r) << "\n";
    }
    md << "\n";
  }

  // --- reproducibility ----------------------------------------------------
  const G4AgentProject::Command build = fProject.EffectiveBuildCommand();
  const G4AgentProject::Command run = fProject.EffectiveRunCommand();
  md << "## Reproducibility\n\n";
  md << "- toolkit: " << G4Version << "\n";
  md << "- build command: " << (build.text.empty() ? G4String("unconfigured") : build.text)
     << " (" << (build.source.empty() ? G4String("unknown") : build.source) << ")\n";
  md << "- run command: " << (run.text.empty() ? G4String("unconfigured") : run.text)
     << " (" << (run.source.empty() ? G4String("unknown") : run.source) << ")\n";
  md << "- runs: ";
  if (runIds.empty())
    md << "none";
  else
    for (std::size_t i = 0; i < runIds.size(); ++i)
    {
      if (i) md << ", ";
      md << runIds[i];
    }
  md << "\n";
  return G4String(md.str());
}

G4String G4AgentPublication::Publish() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] publish");
  if (!fProject.Usable())
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }
  const G4String text = Manuscript();
  G4String abs;
  if (!fProject.ResolveInside("paper/manuscript.md", abs))
  {
    report.AddLine("error : output path outside project root");
    return report.AsString();
  }
  std::error_code ec;
  G4fs::create_directories(G4fs::path(std::string{abs}).parent_path(), ec);
  std::ofstream out(std::string{abs}, std::ios::binary | std::ios::trunc);
  if (!out)
  {
    report.AddLine("error : cannot write paper/manuscript.md");
    return report.AsString();
  }
  out << text;
  out.close();
  if (!out)
  {
    report.AddLine("error : write to paper/manuscript.md failed");
    return report.AsString();
  }
  report.AddLine("file   : paper/manuscript.md");
  report.AddLine("bytes  : " + std::to_string(text.size()));
  report.AddLine(
    "note   : sections are tagged by provenance ([operator]/[assumption]/"
    "[simulated]/[derived]/[literature]); absent inputs are NOT AVAILABLE");
  report.AddLine(
    "note   : no citation, result or uncertainty is fabricated; literature "
    "is drawn only from the references corpus");
  return report.AsString();
}