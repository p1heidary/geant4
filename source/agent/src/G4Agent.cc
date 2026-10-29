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

#include "G4Agent.hh"

#include "G4AgentLLMFactory.hh"
#include "G4AgentPolicy.hh"
#include "G4AgentReport.hh"
#include "G4UIcommandStatus.hh"
#include "G4UImanager.hh"
#include "G4Version.hh"

#include <ctime>
#include <sstream>

G4String G4Agent::Status() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] status");
  report.AddBlock(StatusFields());
  return report.AsString();
}

G4String G4Agent::Version() const
{
  return fInspect.VersionBlock();
}

G4String G4Agent::Capabilities() const
{
  return fInspect.CapabilitiesBlock();
}

void G4Agent::ListUICommands(const G4String& directory) const
{
  fInspect.PrintUIList(directory);
}

G4String G4Agent::ListParticles() const
{
  return fInspect.ParticleListBlock();
}

G4String G4Agent::ListProcesses() const
{
  return fInspect.ProcessListBlock();
}

G4String G4Agent::ListMaterials() const
{
  return fInspect.MaterialListBlock();
}

G4String G4Agent::ListVolumes() const
{
  return fInspect.VolumeListBlock();
}

G4String G4Agent::InspectApplication() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect application");
  report.AddBlock(ApplicationFields());
  return report.AsString();
}

G4String G4Agent::InspectGeometry(const G4String& scope) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect geometry");
  report.AddBlock(fInspect.GeometryBlock(scope));
  return report.AsString();
}

G4String G4Agent::InspectPhysics() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect physics");
  report.AddBlock(PhysicsFields());
  return report.AsString();
}

G4String G4Agent::InspectRun() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect run");
  report.AddBlock(RunFields());
  return report.AsString();
}

G4String G4Agent::InspectHistory() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect history");
  report.AddBlock(HistoryBlock());
  return report.AsString();
}

G4String G4Agent::InspectTracking() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] inspect tracking");
  report.AddBlock(fInspect.TrackingBlock());
  return report.AsString();
}

G4String G4Agent::CheckGeometry(const G4String& scope) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] check geometry");
  report.AddBlock(fCheck.GeometryCheck(scope));
  return report.AsString();
}

G4String G4Agent::CheckPhysics() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] check physics (structural only)");
  report.AddBlock(fCheck.PhysicsCheck());
  return report.AsString();
}

G4String G4Agent::CheckRun() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] check run (structural only)");
  report.AddBlock(fCheck.RunCheck());
  return report.AsString();
}

G4String G4Agent::Report() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] report begin");
  report.AddLine("[status]");
  report.AddBlock(StatusFields());
  report.AddLine("[application]");
  report.AddBlock(ApplicationFields());
  report.AddLine("[physics]");
  report.AddBlock(PhysicsFields());
  report.AddLine("[geometry]");
  report.AddBlock(GeometryFields());
  report.AddLine("[run]");
  report.AddBlock(RunFields());
  report.AddLine("[history]");
  report.AddBlock(HistoryBlock());
  report.AddLine("[checks]");
  report.AddBlock(fCheck.GeometryCheck("world"));
  report.AddBlock(fCheck.PhysicsCheck());
  report.AddBlock(fCheck.RunCheck());
  report.AddLine("[G4AGENT] report end");
  return report.AsString();
}

namespace
{
struct ToolEntry
{
    G4String fName;
    G4String fDescription;
    G4bool fReadOnly = true;  // observation only (no project/state change)
    G4bool fRequiresEdit = false;  // modifies project files: needs EDIT level
};

// Semantic tool registry. Runtime tools are backed by G4AgentInspect/
// G4AgentCheck (the same services the UI commands use); project tools by
// G4AgentProject (path confined). project_write is actuating
// tool: registered with fRequiresEdit and refused below EDIT level by the
// policy gate. list_ui is intentionally absent: its UI facility renders rich
// command documentation for humans/terminals, and a second text
// re-implementation for the model would duplicate an existing Geant4
// capability. Tool results for the loop come only from here.
const std::vector<ToolEntry> kTools = {
  {"status", "runtime status: versions, state, run manager, MT, UI, vis"},
  {"version", "Geant4 toolkit and agent interface versions"},
  {"capabilities", "capabilities implemented by this agent build"},
  {"list_particles", "registered particle types with PDG codes"},
  {"list_processes", "registered process names"},
  {"list_materials", "registered materials with densities"},
  {"list_volumes", "registered physical volumes with solid types"},
  {"inspect_application", "application lifecycle and component presence"},
  {"inspect_geometry",
   "geometry summary/world or a named volume; argument: scope (summary|world|"
   "volume name), optional"},
  {"inspect_physics", "physics list, constructors, production cuts, process categories"},
  {"inspect_run", "run manager and current run state"},
  {"inspect_history", "UI command history"},
  {"inspect_tracking", "tracking availability"},
  {"check_geometry", "overlap check; argument: world or volume name, optional"},
  {"check_physics", "structural physics sanity check (not scientific)"},
  {"check_run", "structural run-manager sanity check"},
  {"report", "aggregated runtime snapshot (sections + checks)"},
  {"validate",
   "validation verdict over existing checks; argument: configuration | runtime"
   " (default both); mechanical only, never scientific"},
  {"knowledge_status", "local knowledge corpus availability (optional)", true, false},
  {"knowledge_search", "search the local knowledge corpus; argument: text to find", true, false},
  {"knowledge_catalog", "list corpus documents with source/version metadata", true, false},
  {"knowledge_gap", "record an unanswered question in the corpus GAPS.md; argument: description",
   true, false},
  {"project_list", "list files under the project root; argument: directory, optional"},
  {"project_read", "read one text file (capped); argument: file path"},
  {"project_search",
   "find a literal pattern; arguments: pattern then optional directory, "
   "separated by a space"},
  {"project_diff", "VCS change view for the project; argument: path, optional"},
  {"project_write",
   "create/replace one text file under the project root; argument: "
   "<path> :: <content>; requires G4_AGENT_PERMISSION=EDIT; journal-recorded",
   false, true},
  {"run_macro",
   "execute a macro file under the project root (controlled simulation); "
   "argument: macro path; journal-recorded",
   true, false},
  {"project_build",
   "run the project build (G4_AGENT_BUILD_COMMAND or a detected CMake/GNUmake "
   "command) in the project root; takes no arguments; requires "
   "G4_AGENT_PERMISSION=EDIT; journal-recorded",
   false, true},
  {"runs_list", "list recorded simulation runs under the project root", true, false},
  {"runs_show", "show one run record/manifest; argument: run id", true, false},
  {"run_project",
   "execute the project application (G4_AGENT_RUN_COMMAND or a detected "
   "executable target) with a path-confined macro and record a reproducible "
   "run under runs/NNN/; argument: macro path; requires "
   "G4_AGENT_PERMISSION=EDIT",
   false, true},
  {"analysis_status", "analysis configuration state (observable/reference specs)", true, false},
  {"analyze_run",
   "extract observables from a recorded run log via validation/observables.txt; "
   "argument: run id; values are never guessed",
   true, false},
  {"compare_run",
   "compare a run's extracted observables against validation/references.txt "
   "with CONSISTENT|CONTRARY|INCONCLUSIVE|NOT COMPARABLE; argument: run id; "
   "deterministic numeric comparison only, never scientific acceptance",
   true, false},
  {"references_status", "literature reference corpus availability", true, false},
  {"references_list", "list reference records (id, year, authors, title, doi/arXiv)", true, false},
  {"reference_show", "show one reference's declared fields; argument: reference id", true, false},
  {"reference_cite", "format a citation from declared fields only; argument: reference id", true,
   false},
  {"manuscript_preview",
   "assemble the provenance-tagged manuscript draft without writing; absent "
   "inputs are NOT AVAILABLE",
   true, false},
  {"publish",
   "write the provenance-tagged manuscript to paper/manuscript.md; requires "
   "G4_AGENT_PERMISSION=EDIT; never fabricates results or citations",
   false, true}};

G4String timeText()
{
  std::time_t now = std::time(nullptr);
  char buf[64] = {0};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  return G4String(buf);
}

constexpr std::size_t kBuildOutCap = 64 * 1024;

G4String FirstLine(const G4String& text)
{
  const auto nl = text.find('\n');
  G4String line = text;
  if (nl != G4String::npos) line = text.substr(0, nl);
  if (line.size() > 120) line = line.substr(0, 117) + "...";
  return line;
}
}  // namespace

G4Agent::G4Agent()
  : fValidate(fCheck, fInspect),
    fProject(G4AgentProject::FromEnvironment()),
    fKnowledge(G4AgentKnowledge::ConfiguredCorpusDir(fProject.Root()), G4String(G4Version)),
    fRuns(fProject),
    fAnalysis(fProject),
    fPublication(fProject, fAnalysis)
{}

const std::vector<G4String>& G4Agent::ToolNames()
{
  static const std::vector<G4String> names = []() {
    std::vector<G4String> out;
    for (const auto& t : kTools)
      out.push_back(t.fName);
    return out;
  }();
  return names;
}

G4String G4Agent::ToolDescription(const G4String& name)
{
  for (const auto& t : kTools)
    if (t.fName == name) return t.fDescription;
  return "";
}

const ToolEntry* FindToolEntry(const G4String& name)
{
  for (const auto& t : kTools)
    if (t.fName == name) return &t;
  return nullptr;
}

G4bool G4Agent::IsKnownTool(const G4String& name)
{
  return FindToolEntry(name) != nullptr;
}

G4bool G4Agent::ToolIsReadOnly(const G4String& name)
{
  const ToolEntry* entry = FindToolEntry(name);
  return entry != nullptr && entry->fReadOnly;
}

G4bool G4Agent::ToolRequiresEdit(const G4String& name)
{
  const ToolEntry* entry = FindToolEntry(name);
  return entry != nullptr && entry->fRequiresEdit;
}

G4String G4Agent::ExecuteTool(const G4String& name, const G4String& argument) const
{
  if (name == "status")              return Status();
  if (name == "version")             return Version();
  if (name == "capabilities")        return Capabilities();
  if (name == "list_particles")      return ListParticles();
  if (name == "list_processes")      return ListProcesses();
  if (name == "list_materials")      return ListMaterials();
  if (name == "list_volumes")        return ListVolumes();
  if (name == "inspect_application") return InspectApplication();
  if (name == "inspect_geometry")    return InspectGeometry(argument);
  if (name == "inspect_physics")     return InspectPhysics();
  if (name == "inspect_run")         return InspectRun();
  if (name == "inspect_history")     return InspectHistory();
  if (name == "inspect_tracking")    return InspectTracking();
  if (name == "check_geometry")      return CheckGeometry(argument.empty() ? "world" : argument);
  if (name == "check_physics")     return CheckPhysics();
  if (name == "check_run")         return CheckRun();
  if (name == "report")            return Report();
  if (name == "validate")          return Validate(argument);
  if (name == "knowledge_status")  return Knowledge("status", argument);
  if (name == "knowledge_search")  return Knowledge("search", argument);
  if (name == "knowledge_catalog") return Knowledge("catalog", argument);
  if (name == "knowledge_gap")     return Knowledge("gap", argument);
  if (name == "project_list")      return fProject.ListFiles(argument);
  if (name == "project_read")      return fProject.ReadFile(argument);
  if (name == "project_search")
  {
    auto sp = argument.find(' ');
    if (sp == G4String::npos) return fProject.Search(argument, "");
    return fProject.Search(argument.substr(0, sp), argument.substr(sp + 1));
  }
  if (name == "project_diff") return fProject.Diff(argument);
  if (name == "project_write")
  {
    auto sep = argument.find(" :: ");
    if (sep == G4String::npos) return "[G4AGENT] project write\nerror : usage <path> :: <content>";
    return fProject.WriteFile(argument.substr(0, sep), argument.substr(sep + 4));
  }
  if (name == "run_macro")          return RunMacroFile(argument);
  if (name == "project_build")      return RunProjectBuild();
  if (name == "runs_list")          return ListRuns();
  if (name == "runs_show")          return ShowRun(argument);
  if (name == "run_project")        return RunProject(argument);
  if (name == "analysis_status")    return Analysis("status", argument);
  if (name == "analyze_run")        return Analysis("analyze", argument);
  if (name == "compare_run")        return Analysis("compare", argument);
  if (name == "references_status")  return Bibliography("status", argument);
  if (name == "references_list")    return Bibliography("list", argument);
  if (name == "reference_show")     return Bibliography("show", argument);
  if (name == "reference_cite")     return Bibliography("cite", argument);
  if (name == "manuscript_preview") return Manuscript();
  if (name == "publish")            return Publish();
  return "[G4AGENT] tool error\nunknown tool : " + name;
}

G4String G4Agent::Knowledge(const G4String& subcommand, const G4String& argument) const
{
  return fKnowledge.Run(subcommand, argument);
}

G4String G4Agent::Validate(const G4String& tier) const
{
  return fValidate.Validate(tier);
}

G4String G4Agent::RunMacroFile(const G4String& macroPath) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] run macro");
  if (macroPath.empty())
  {
    report.AddLine("error : macro path required");
    return report.AsString();
  }
  G4String abs;
  if (!fProject.ResolveInside(macroPath, abs))
  {
    report.AddLine("error : path outside project root");
    return report.AsString();
  }

  // Reuse the existing UI facility rather than a second macro engine: the
  // master UI manager executes the file exactly as an interactive user
  // would (/control/execute), including all standard commands.
  auto* ui = G4UImanager::GetUIpointer();
  if (ui == nullptr)
  {
    report.AddLine("error : UI manager unavailable");
    return report.AsString();
  }
  const auto rc = ui->ApplyCommand("/control/execute " + abs);
  report.AddLine("macro  : " + macroPath);
  report.AddLine(
    "status : "
    + (rc == fCommandSucceeded ? "completed" : "finished with error code " + std::to_string(rc)));
  report.AddLine(
    "note   : macro output was streamed to the application log;"
    " results require external analysis (VALIDATION.md)");
  return report.AsString();
}

G4String G4Agent::RunProjectBuild() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] project build");
  const G4AgentProject::Command build = fProject.EffectiveBuildCommand();
  if (build.text.empty())
  {
    report.AddLine(
      "error : build command not configured"
      " (set G4_AGENT_BUILD_COMMAND)");
    if (!build.reason.empty()) report.AddLine("reason: " + build.reason);
    return report.AsString();
  }
  const G4String cmd = build.text;
#ifndef _WIN32
  // Fixed operator/detected command, executed with the project root as
  // working directory; the model cannot contribute arguments (fail-closed
  // design: "limit shell execution"). The subshell keeps chained
  // commands (for example configure && build) inside the captured stream.
  const G4String full = "cd " + G4String("'") + G4AgentProject::SanitizeForShell(fProject.Root())
                        + "'" + " && ( " + cmd + " ) 2>&1";
  FILE* pipe = popen(full.c_str(), "r");
  if (pipe == nullptr)
  {
    report.AddLine("error : build execution failed to start");
    return report.AsString();
  }
  std::string out;
  char buf[4096];
  std::size_t n = 0;
  bool truncated = false;
  while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
  {
    if (out.size() + n > kBuildOutCap)
    {
      truncated = true;
      break;
    }
    out.append(buf, n);
  }
  const int status = pclose(pipe);
  report.AddLine("command: " + cmd);
  report.AddLine("source : " + build.source);
  report.AddLine("status : " + std::to_string(status));
  std::istringstream lines(out);
  std::string line;
  while (std::getline(lines, line))
    report.AddLine(line);
  if (truncated) report.AddLine("note : output truncated");
#else
  report.AddLine("note : build execution requires a POSIX platform in this build");
#endif
  return report.AsString();
}

G4String G4Agent::RunProject(const G4String& macro) const
{
  return fRuns.RunProject(macro);
}

G4String G4Agent::ListRuns() const
{
  return fRuns.ListRuns();
}

G4String G4Agent::ShowRun(const G4String& id) const
{
  return fRuns.ShowRun(id);
}

G4String G4Agent::Analysis(const G4String& subcommand, const G4String& argument) const
{
  return fAnalysis.Run(subcommand, argument);
}

G4String G4Agent::Bibliography(const G4String& subcommand, const G4String& argument) const
{
  return fPublication.Bibliography(subcommand, argument);
}

G4String G4Agent::Manuscript() const
{
  return fPublication.Manuscript();
}

G4String G4Agent::Publish() const
{
  return fPublication.Publish();
}

G4String G4Agent::LLMStatus() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] llm");
  auto config = G4AgentLLMConfig::FromEnvironment();
  report.AddLine("llm        : " + config.Describe());
  report.AddLine("model      : " + (config.Model().empty() ? "unconfigured" : config.Model()));
  report.AddLine("provider   : "
                 + (config.Provider().empty() ? "unconfigured" : config.Provider()));
  report.AddLine("timeout    : " + std::to_string(config.TimeoutSeconds()) + " s");
  report.AddLine("max-steps  : " + std::to_string(config.MaxSteps()));
  report.AddLine("permission : " + G4AgentPolicy::FromString(config.Permission()).Name());
  report.AddLine("tools      : " + std::to_string(ToolNames().size()) + " (read-only)");
  const G4String configuredRoot = G4AgentProject::ConfiguredRoot();
  G4String projectLine = fProject.Root();
  if (configuredRoot.empty()) projectLine += " (working directory)";
  report.AddLine("project    : " + projectLine);
  const G4AgentProject::Command build = fProject.EffectiveBuildCommand();
  const G4AgentProject::Command run = fProject.EffectiveRunCommand();
  report.AddLine("build-cmd  : " + (build.text.empty() ? G4String("unconfigured") : build.text));
  if (!build.text.empty()) report.AddLine("build-src  : " + build.source);
  report.AddLine("run-cmd    : " + (run.text.empty() ? G4String("unconfigured") : run.text));
  if (!run.text.empty()) report.AddLine("run-src    : " + run.source);
  return report.AsString();
}

G4String G4Agent::Ask(const G4String& question) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] ask");

  auto config = G4AgentLLMConfig::FromEnvironment();
  auto backend = G4AgentLLMFactory::Create(config);
  if (backend == nullptr)
  {
    report.AddLine("answer     : LLM unavailable (provider unconfigured or module absent)");
    report.AddLine(
      "note   : configure G4_AGENT_PROVIDER/G4_AGENT_MODEL/G4_AGENT_BASE_URL"
      " and link the optional G4agent_llm module for real providers;"
      " native commands work without an LLM.");
    return report.AsString();
  }

  if (question.empty())
  {
    report.AddLine("error      : empty question");
    return report.AsString();
  }

  const G4AgentPolicy policy = G4AgentPolicy::FromString(config.Permission());
  const G4int maxSteps = config.MaxSteps();

  // Tool surface = the semantic registry; all registered tools are
  // read-only observation/checking services.
  std::vector<G4LLMToolSpec> tools;
  for (const auto& name : ToolNames())
    tools.push_back({name, ToolDescription(name)});

  std::vector<G4LLMMessage> messages;
  G4LLMMessage systemMsg;
  systemMsg.fRole = G4LLMRole::System;
  systemMsg.fContent =
    "You are the G4Agent assistant for a running Geant4 application. "
    "Runtime tools observe the simulation; project tools inspect a confined "
    "project directory (G4_AGENT_PROJECT_DIR) with read-only path "
    "resolution. project_write creates/replaces text files only when the "
    "operator granted G4_AGENT_PERMISSION=EDIT, and every modification is "
    "journal-recorded; run_macro executes confined macros. Unknown or "
    "ungranted tools are refused; tool results are plain text. Never claim "
    "scientific validity from structural checks; treat check results as "
    "evidence, not proof. Finish with a final text answer when the evidence "
    "is sufficient.";
  messages.push_back(systemMsg);

  G4LLMMessage userMsg;
  userMsg.fRole = G4LLMRole::User;
  userMsg.fContent = question;
  messages.push_back(userMsg);

  report.AddLine("backend    : " + backend->Name());
  report.AddLine("permission : " + policy.Name());

  G4int steps = 0;
  G4bool finalReached = false;
  G4String finalText;

  while (steps < maxSteps)
  {
    ++steps;
    auto reply = backend->Complete(messages, tools);

    if (!reply.fOk)
    {
      report.AddLine("error      : " + reply.fError);
      report.AddLine("steps      : " + std::to_string(steps));
      return report.AsString();
    }

    if (reply.fToolCalls.empty())
    {
      finalText = reply.fText;
      finalReached = true;
      break;
    }

    // Record the assistant turn (with its requested tool calls) and feed
    // every tool result back as a tool-role message.
    G4LLMMessage assistant;
    assistant.fRole = G4LLMRole::Assistant;
    assistant.fContent = reply.fText;
    assistant.fToolCalls = reply.fToolCalls;
    messages.push_back(assistant);

    for (const auto& call : reply.fToolCalls)
    {
      // Policy gate: registered tools are read-only and allowed; unknown
      // names are treated as not-read-only and refused (fail closed).
      // Permission consult is registry-driven: unknown names are treated as
      // modifying and refused fail-closed; actuating tools need EDIT.
      const ToolEntry* entry = FindToolEntry(call.fName);
      const G4String refusal = policy.CheckTool(call.fName, entry != nullptr && entry->fReadOnly,
                                                entry != nullptr && entry->fRequiresEdit);

      G4String result;
      if (!refusal.empty())
      {
        result = refusal;
        report.AddLine("tool       : " + call.fName + " -> refused (policy)");
      }
      else
      {
        result = ExecuteTool(call.fName, call.fArgument);
        report.AddLine("tool       : " + call.fName
                       + (call.fArgument.empty() ? "" : " [" + call.fArgument + "]") + " -> ok");
        if (entry != nullptr && (!entry->fReadOnly || entry->fName == "run_macro"))
        {
          // Retain hypothesis/change/test/result, human-readable,
          // inside the project root.
          std::ostringstream journal;
          journal << "\n## " << timeText() << "\n\n"
                  << "- hypothesis: " << reply.fText << "\n"
                  << "- change: " << call.fName << " " << call.fArgument << "\n"
                  << "- test: run /agent/check and /agent/report after the "
                     "change; rebuild and rerun as applicable\n"
                  << "- result: " << FirstLine(result) << "\n";
          fProject.JournalAppend(G4String(journal.str()));
        }
      }

      G4LLMMessage toolMsg;
      toolMsg.fRole = G4LLMRole::Tool;
      toolMsg.fToolCallId = call.fId;
      toolMsg.fContent = result;
      messages.push_back(toolMsg);
    }
  }

  if (!finalReached)
  {
    finalText = "no final answer within loop bound (" + std::to_string(maxSteps) + " model steps)";
  }

  report.AddLine("steps      : " + std::to_string(steps));
  report.AddLine("answer     : " + finalText);
  return report.AsString();
}

// Section renderers (shared with Report(); delegate to the inspect service)

G4String G4Agent::StatusFields() const
{
  return fInspect.StatusFields();
}

G4String G4Agent::ApplicationFields() const
{
  return fInspect.ApplicationFields();
}

G4String G4Agent::PhysicsFields() const
{
  return fInspect.PhysicsFields();
}

G4String G4Agent::GeometryFields() const
{
  return fInspect.GeometryFields();
}

G4String G4Agent::RunFields() const
{
  return fInspect.RunFields();
}

G4String G4Agent::HistoryBlock() const
{
  return fInspect.HistoryBlock();
}
