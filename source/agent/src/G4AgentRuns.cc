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

#include "G4AgentRuns.hh"

#include "G4AgentProject.hh"
#include "G4AgentReport.hh"
#include "G4Filesystem.hh"
#include "G4Version.hh"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
G4fs::path P(const G4String& x)
{
  return G4fs::path{std::string{x}};
}

constexpr std::size_t kMaxLogBytes = 1024 * 1024;
constexpr const char* kRunsDir = "runs";

G4String Now()
{
  std::time_t now = std::time(nullptr);
  char buf[64] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  return G4String(buf);
}

G4String ZeroPad(int value)
{
  std::ostringstream os;
  os.width(3);
  os.fill('0');
  os << value;
  return G4String(os.str());
}

// Next free run id: highest existing numeric directory + 1, or 001.
G4String NextRunId(const G4fs::path& runsRoot)
{
  int maxId = 0;
  std::error_code ec;
  if (G4fs::is_directory(runsRoot, ec) && !ec)
  {
    for (const auto& entry : G4fs::directory_iterator(runsRoot, ec))
    {
      if (!entry.is_directory(ec)) continue;
      const std::string name = entry.path().filename().string();
      if (name.empty() || std::find_if(name.begin(), name.end(), [](unsigned char c) {
                            return std::isdigit(c) == 0;
                          }) != name.end())
        continue;
      try
      {
        maxId = std::max(maxId, std::stoi(name));
      }
      catch (...)
      {
        continue;
      }
    }
  }
  return ZeroPad(maxId + 1);
}

// Best-effort source revision via a fixed read-only git command; never fatal.
G4String SourceRevision(const G4String& root)
{
#ifndef _WIN32
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
  const G4String cmd =
    "env -i LC_ALL=C git -C " + quote(root) + " rev-parse --short HEAD 2>/dev/null";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) return "unknown";
  char buf[128] = {0};
  std::string out;
  if (fgets(buf, sizeof(buf), pipe) != nullptr) out = buf;
  pclose(pipe);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out.empty() ? "unknown" : G4String(out);
#else
  (void)root;
  return "unknown";
#endif
}
}  // namespace

G4AgentRuns::G4AgentRuns(const G4AgentProject& project) : fProject(project) {}

G4String G4AgentRuns::RunProject(const G4String& macro) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] run project");

  if (!fProject.Usable())
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }

  const G4AgentProject::Command run = fProject.EffectiveRunCommand();
  if (run.text.empty())
  {
    report.AddLine("error : run command not configured (set G4_AGENT_RUN_COMMAND)");
    if (!run.reason.empty()) report.AddLine("reason: " + run.reason);
    report.AddLine(
      "note  : the harness executes the project application, not its own "
      "embedded run manager");
    return report.AsString();
  }
  const G4String runCmd = run.text;

  G4String macroAbs;
  if (!macro.empty() && !fProject.ResolveInside(macro, macroAbs))
  {
    report.AddLine("error : macro path outside project root");
    return report.AsString();
  }

  const G4fs::path runsRoot = P(fProject.Root()) / kRunsDir;
  const G4String id = NextRunId(runsRoot);
  const G4fs::path runDir = runsRoot / std::string{id};
  std::error_code ec;
  G4fs::create_directories(runDir, ec);
  if (ec)
  {
    report.AddLine("error : cannot create run directory");
    return report.AsString();
  }

  const G4String quotedRoot = G4AgentProject::SanitizeForShell(fProject.Root());
  const G4String quotedMacro = G4AgentProject::SanitizeForShell(macroAbs);
  G4String full = "cd '" + quotedRoot + "' && ( " + runCmd;
  if (!macro.empty()) full += " '" + quotedMacro + "'";
  full += " ) 2>&1";

  const auto start = std::chrono::steady_clock::now();
  std::string output;
  int status = -1;
  bool truncated = false;
#ifndef _WIN32
  FILE* pipe = popen(full.c_str(), "r");
  if (pipe != nullptr)
  {
    char buf[4096];
    std::size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
    {
      if (output.size() + n > kMaxLogBytes)
      {
        truncated = true;
        break;
      }
      output.append(buf, n);
    }
    status = pclose(pipe);
  }
#endif
  const auto end = std::chrono::steady_clock::now();
  const double seconds = std::chrono::duration<double>(end - start).count();

  // Persist the captured log verbatim.
  const G4fs::path logPath = runDir / "log.txt";
  {
    std::ofstream log(std::string{logPath.string()}, std::ios::binary | std::ios::trunc);
    if (log)
    {
      log << output;
      if (truncated) log << "\n[log truncated at " << kMaxLogBytes << " bytes]\n";
    }
  }

  // Deterministic manifest; no timestamps in the *report*, but the record is
  // a reproducibility artifact and legitimately carries one.
  const G4AgentProject::Command build = fProject.EffectiveBuildCommand();
  std::ostringstream manifest;
  manifest << "[G4AGENT] run manifest\n";
  manifest << "run            : " << id << "\n";
  manifest << "timestamp      : " << Now() << "\n";
  manifest << "project        : " << fProject.Root() << "\n";
  manifest << "geant4         : " << G4Version << "\n";
  manifest << "source revision: " << SourceRevision(fProject.Root()) << "\n";
  manifest << "build command  : "
           << (build.text.empty() ? G4String("unconfigured") : build.text) << "\n";
  manifest << "build source   : "
           << (build.source.empty() ? G4String("unknown") : build.source) << "\n";
  manifest << "run command    : " << runCmd << "\n";
  manifest << "run source     : " << (run.source.empty() ? G4String("unknown") : run.source)
           << "\n";
  manifest << "macro          : " << (macro.empty() ? G4String("none") : macro) << "\n";
  manifest << "exit status    : " << status << "\n";
  manifest << "duration       : " << seconds << " s\n";
  manifest << "log            : runs/" << id << "/log.txt\n";
  manifest << "outputs        : not tracked automatically (external analysis)\n";
  manifest << "observables    : not extracted (requires external analysis)\n";
  manifest << "limitations    : fields are recorded as observed; unavailable "
              "values are marked unknown\n";

  const G4fs::path manifestPath = runDir / "manifest.txt";
  {
    std::ofstream out(std::string{manifestPath.string()}, std::ios::binary | std::ios::trunc);
    if (!out)
    {
      report.AddLine("error : cannot write run manifest");
      return report.AsString();
    }
    out << manifest.str();
  }

  report.AddLine("run      : " + id);
  report.AddLine("macro    : " + (macro.empty() ? G4String("none") : macro));
  report.AddLine("command  : " + runCmd);
  report.AddLine("source   : " + run.source);
  report.AddLine("status   : " + std::to_string(status));
  report.AddLine("duration : " + std::to_string(seconds) + " s");
  report.AddLine("manifest : runs/" + id + "/manifest.txt");
  report.AddLine("log      : runs/" + id + "/log.txt");
  if (truncated) report.AddLine("note     : captured log truncated");
  report.AddLine(
    "note     : observables are not extracted; physics results require "
    "external analysis (no scientific validation is implied)");
  return report.AsString();
}

G4String G4AgentRuns::ListRuns() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] runs list");
  if (!fProject.Usable())
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }
  const G4fs::path runsRoot = P(fProject.Root()) / kRunsDir;
  std::error_code ec;
  if (!G4fs::is_directory(runsRoot, ec) || ec)
  {
    report.AddLine("root  : " + fProject.Root());
    report.AddLine("count : 0");
    report.AddLine("note  : no runs recorded yet");
    return report.AsString();
  }

  std::vector<std::string> ids;
  for (const auto& entry : G4fs::directory_iterator(runsRoot, ec))
  {
    if (!entry.is_directory(ec)) continue;
    ids.push_back(entry.path().filename().string());
  }
  std::sort(ids.begin(), ids.end());

  report.AddLine("root  : " + fProject.Root());
  report.AddLine("count : " + std::to_string(ids.size()));
  for (const auto& id : ids)
  {
    // Summarize from the manifest header lines without dumping the file.
    G4String timestamp = "unknown";
    G4String status = "unknown";
    G4String macro = "unknown";
    std::ifstream in(std::string{(runsRoot / id / "manifest.txt").string()});
    std::string line;
    while (std::getline(in, line))
    {
      // Fields are "label<padding> : value"; match the label then take the
      // text after the first colon.
      auto field = [&](const char* key, G4String& out) {
        const std::string k(key);
        if (line.rfind(k, 0) != 0) return;
        const auto colon = line.find(':', k.size());
        if (colon == std::string::npos) return;
        std::size_t i = colon + 1;
        while (i < line.size() && line[i] == ' ')
          ++i;
        out = G4String(line.substr(i));
      };
      field("timestamp", timestamp);
      field("exit status", status);
      field("macro", macro);
    }
    report.AddLine("  " + G4String(id) + "  " + timestamp + "  status " + status + "  " + macro);
  }
  return report.AsString();
}

G4String G4AgentRuns::ShowRun(const G4String& id) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] runs show");
  if (!fProject.Usable())
  {
    report.AddLine("error : project root unavailable");
    return report.AsString();
  }
  if (id.empty())
  {
    report.AddLine("error : run id required");
    return report.AsString();
  }

  G4String manifestAbs;
  if (!fProject.ResolveInside(kRunsDir + G4String("/") + id + "/manifest.txt", manifestAbs))
  {
    report.AddLine("error : run id outside project root");
    return report.AsString();
  }
  std::ifstream in(std::string{manifestAbs});
  if (!in)
  {
    report.AddLine("error : no run record '" + id + "'");
    return report.AsString();
  }
  std::string line;
  while (std::getline(in, line))
    report.AddLine(line);
  return report.AsString();
}