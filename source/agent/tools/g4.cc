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
/// \file g4.cc
/// \brief The g4 Geant4 coding/scientific harness command-line interface.
///
/// This binary contains no Geant4-implementing logic of its own. It embeds a
/// minimal run-manager application (so that runtime inspection has real state
/// to observe), activates the native G4Agent services through G4AgentManager,
/// and forwards every CLI operation to those SAME services:
///
///   * read-only inspection/checking is dispatched through
///     G4AgentManager::ExecuteTool (the same registry the LLM tool loop uses);
///   * the LLM-backed operations reuse G4AgentManager::Ask;
///   * UI listing defers to the live G4UImanager, exactly as /agent/list/ui.
///
/// Modes (one command system, two entry points):
///   g4                      interactive session (prompt "g4>")
///   g4 /status              run one command non-interactively
///   g4 "<natural language>" non-interactive agent question (same as /ask)
///
/// Slash commands start with '/'; any other line is natural language and is
/// sent to the configured agent backend. Run /help for the command list.
///
/// Exit status: 0 on success; 2 on a usage/unknown-command error. A command
/// that runs but produces no evidence never reports success (see the agent
/// reports themselves).

#include "G4AgentManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace
{
//----------------------------------------------------------------------------
// Small deterministic text helpers (no external dependencies).
//----------------------------------------------------------------------------
std::string Trim(const std::string& s)
{
  std::size_t a = 0;
  std::size_t b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])) != 0)
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])) != 0)
    --b;
  return s.substr(a, b - a);
}

// Split off the first whitespace-delimited token; `rest` is the remainder,
// trimmed of surrounding whitespace.
void SplitHead(const std::string& s, std::string& head, std::string& rest)
{
  std::size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])) == 0)
    ++i;
  head = s.substr(0, i);
  rest = Trim(s.substr(i));
}

// Remove one surrounding pair of double quotes, if present.
std::string StripQuotes(const std::string& s)
{
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
  return s;
}

//----------------------------------------------------------------------------
// The CLI: one dispatcher shared by interactive and non-interactive modes.
//----------------------------------------------------------------------------
class G4Cli
{
  public:

    explicit G4Cli(G4AgentManager& agent) : fAgent(agent) {}

    // Prints the session banner. Identifies the Geant4 installation only:
    // there is deliberately no independent agent product version.
    void Banner() const
    {
      std::cout << "Geant4 " << G4AgentManager::GetGeant4Version() << "\n";
      std::cout << "Geant4 coding/scientific harness. Natural language is sent to "
                   "the agent.\n";
      std::cout << "Type /help for commands, /exit to leave.\n";
    }

    // Interactive loop: read a line, dispatch it, repeat until /exit or EOF.
    void Repl()
    {
      fInteractive = true;
      std::string line;
      while (!fQuit)
      {
        std::cout << "g4> " << std::flush;
        if (!std::getline(std::cin, line))
        {
          std::cout << "\n";
          break;  // EOF / cancellation
        }
        RunLine(line);
      }
    }

    // Dispatch exactly one command line. Returns the process exit code
    // contribution: 0 for success, 2 for usage/unknown-command errors.
    int RunLine(const std::string& raw)
    {
      const std::string line = Trim(raw);
      if (line.empty()) return 0;

      if (line[0] != '/')
      {
        // Natural language: same backend/tool loop as /ask.
        Emit(fAgent.Ask(line));
        return 0;
      }

      std::string cmd;
      std::string args;
      SplitHead(line, cmd, args);

      if (cmd == "/exit" || cmd == "/quit")
      {
        fQuit = true;
        if (fInteractive) std::cout << "bye\n";
        return 0;
      }

      if (cmd == "/help")         return CmdHelp(args);
      if (cmd == "/status")       return EmitTool("status", "");
      if (cmd == "/version")      return EmitTool("version", "");
      if (cmd == "/capabilities") return EmitTool("capabilities", "");
      if (cmd == "/report")       return EmitTool("report", "");
      if (cmd == "/llm")          return CmdLlm();
      if (cmd == "/inspect")      return CmdInspect(args);
      if (cmd == "/check")        return CmdCheck(args);
      if (cmd == "/list")         return CmdList(args);
      if (cmd == "/validate")     return EmitTool("validate", args);
      if (cmd == "/knowledge")    return CmdKnowledge(args);
      if (cmd == "/run")          return CmdRun(args);
      if (cmd == "/runs")         return CmdRuns(args);
      if (cmd == "/analyze")      return CmdAnalyze(args);
      if (cmd == "/compare")      return CmdCompare(args);
      if (cmd == "/references")   return CmdReferences(args);
      if (cmd == "/manuscript")   return EmitTool("manuscript_preview", "");
      if (cmd == "/publish")      return EmitTool("publish", "");
      if (cmd == "/build")        return CmdBuild(args);
      if (cmd == "/ask")          return CmdAsk(args);
      if (cmd == "/edit")         return CmdEdit(args);

      std::cerr << "unknown command '" << cmd << "'; type /help for the command list\n";
      return 2;
    }

  private:

    void Emit(const G4String& text)
    {
      std::string out(text);
      std::cout << out;
      if (out.empty() || out.back() != '\n') std::cout << "\n";
    }

    int EmitTool(const G4String& tool, const G4String& argument)
    {
      Emit(fAgent.ExecuteTool(tool, argument));
      return 0;
    }

    int CmdLlm()
    {
      // Forward to the registered /agent/llm command (single implementation).
      auto* ui = G4UImanager::GetUIpointer();
      if (ui == nullptr)
      {
        std::cerr << "error: UI manager unavailable\n";
        return 2;
      }
      ui->ApplyCommand("/agent/llm");
      return 0;
    }

    int CmdInspect(const std::string& args)
    {
      std::string what;
      std::string scope;
      SplitHead(args, what, scope);
      if (what.empty())
      {
        std::cerr << "usage: /inspect application|geometry|physics|run|history|tracking "
                     "[scope]\n";
        return 2;
      }
      if (what == "application") return EmitTool("inspect_application", scope);
      if (what == "geometry")    return EmitTool("inspect_geometry", scope);
      if (what == "physics")     return EmitTool("inspect_physics", scope);
      if (what == "run")         return EmitTool("inspect_run", scope);
      if (what == "history")     return EmitTool("inspect_history", scope);
      if (what == "tracking")    return EmitTool("inspect_tracking", scope);
      std::cerr << "unknown inspect target '" << what << "'\n";
      return 2;
    }

    int CmdCheck(const std::string& args)
    {
      std::string what;
      std::string scope;
      SplitHead(args, what, scope);
      if (what == "geometry") return EmitTool("check_geometry", scope);
      if (what == "physics")  return EmitTool("check_physics", scope);
      if (what == "run")      return EmitTool("check_run", scope);
      std::cerr << "usage: /check geometry|physics|run [scope]\n";
      return 2;
    }

    int CmdList(const std::string& args)
    {
      std::string what;
      std::string where;
      SplitHead(args, what, where);
      if (what == "particles") return EmitTool("list_particles", where);
      if (what == "processes") return EmitTool("list_processes", where);
      if (what == "materials") return EmitTool("list_materials", where);
      if (what == "volumes")   return EmitTool("list_volumes", where);
      if (what == "ui")
      {
        auto* ui = G4UImanager::GetUIpointer();
        if (ui == nullptr)
        {
          std::cerr << "error: UI manager unavailable\n";
          return 2;
        }
        ui->ApplyCommand(where.empty() ? G4String("/agent/list/ui")
                                       : G4String("/agent/list/ui " + where));
        return 0;
      }
      std::cerr << "usage: /list ui|particles|processes|materials|volumes [directory]\n";
      return 2;
    }

    int CmdKnowledge(const std::string& args)
    {
      std::string what;
      std::string rest;
      SplitHead(args, what, rest);
      if (what.empty() || what == "status") return EmitTool("knowledge_status", rest);
      if (what == "catalog") return EmitTool("knowledge_catalog", rest);
      if (what == "search")  return EmitTool("knowledge_search", rest);
      if (what == "gap")     return EmitTool("knowledge_gap", rest);
      std::cerr << "usage: /knowledge [status|catalog|search <text>|gap <text>]\n";
      return 2;
    }

    int CmdRun(const std::string& args)
    {
      const std::string macro = StripQuotes(Trim(args));
      if (macro.empty())
      {
        std::cerr << "usage: /run <macro-file>   (path is confined to the project root; "
                     "executes the project application (G4_AGENT_RUN_COMMAND or a detected "
                     "executable) and records a run; requires G4_AGENT_PERMISSION=EDIT)\n";
        return 2;
      }
      return EmitTool("run_project", macro);
    }

    int CmdRuns(const std::string& args)
    {
      std::string what;
      std::string rest;
      SplitHead(args, what, rest);
      if (what.empty() || what == "list") return EmitTool("runs_list", "");
      if (what == "show") return EmitTool("runs_show", rest);
      // Bare run id is a convenience for /runs <id>.
      return EmitTool("runs_show", what);
    }

    int CmdAnalyze(const std::string& args)
    {
      const std::string id = Trim(args);
      if (id.empty())
      {
        std::cerr << "usage: /analyze <run-id>   (extracts observables from the run log via "
                     "validation/observables.txt)\n";
        return 2;
      }
      return EmitTool("analyze_run", id);
    }

    int CmdCompare(const std::string& args)
    {
      const std::string id = Trim(args);
      if (id.empty())
      {
        std::cerr << "usage: /compare <run-id>   (compares extracted observables against "
                     "validation/references.txt)\n";
        return 2;
      }
      return EmitTool("compare_run", id);
    }

    int CmdReferences(const std::string& args)
    {
      std::string what;
      std::string rest;
      SplitHead(args, what, rest);
      if (what.empty() || what == "status") return EmitTool("references_status", rest);
      if (what == "list") return EmitTool("references_list", rest);
      if (what == "show") return EmitTool("reference_show", rest);
      if (what == "cite") return EmitTool("reference_cite", rest);
      std::cerr << "usage: /references [status|list|show <id>|cite <id>]\n";
      return 2;
    }

    int CmdBuild(const std::string& args)
    {
      if (!Trim(args).empty())
      {
        std::cerr << "usage: /build   (takes no arguments; the build command comes from "
                     "G4_AGENT_BUILD_COMMAND or is detected from the current "
                     "project)\n";
        return 2;
      }
      return EmitTool("project_build", "");
    }

    int CmdAsk(const std::string& args)
    {
      const std::string question = StripQuotes(Trim(args));
      if (question.empty())
      {
        std::cerr << "usage: /ask <question>\n";
        return 2;
      }
      Emit(fAgent.Ask(question));
      return 0;
    }

    int CmdEdit(const std::string& args)
    {
      const std::string instruction = StripQuotes(Trim(args));
      if (instruction.empty())
      {
        std::cerr << "usage: /edit \"<change description>\"   (requires an LLM backend "
                     "and G4_AGENT_PERMISSION=EDIT for the change to be applied)\n";
        return 2;
      }
      // An edit is a coding task delegated to the bounded agent loop; the
      // loop can only apply it through the EDIT-gated project_write tool.
      Emit(fAgent.Ask("Apply a minimal, targeted edit to the project: " + instruction
                      + ". Inspect the relevant files first, restrict changes to what is "
                        "necessary, show the result, and report any unresolved issues."));
      return 0;
    }

    int CmdHelp(const std::string& topic)
    {
      const std::string t = Trim(topic);
      if (!t.empty()) return HelpFor(t);
      std::cout << "g4 - Geant4 coding/scientific harness\n"
                   "\n"
                   "Input not starting with '/' is natural language and is sent to the agent\n"
                   "(equivalent to /ask).\n"
                   "\n"
                   "Commands:\n"
                   "  /help [command]      this help, or detail for one command\n"
                   "  /status              runtime status snapshot\n"
                   "  /version             toolkit and agent-interface versions\n"
                   "  /capabilities        capabilities implemented by this build\n"
                   "  /report              aggregated runtime snapshot (sections + checks)\n"
                   "  /llm                 LLM configuration state (no network contact)\n"
                   "  /inspect <what> [scope]  application|geometry|physics|run|history|tracking\n"
                   "  /check <what> [scope]    geometry|physics|run (structural only)\n"
                   "  /list <what> [dir]       ui|particles|processes|materials|volumes\n"
                   "  /validate [tier]         configuration|runtime (default both)\n"
                   "  /knowledge [sub] [arg]   status|catalog|search <text>|gap <text>\n"
                   "  /run <macro>         execute the project application with a confined\n"
                   "                       macro and record a run (env or detected; EDIT)\n"
                   "  /runs [list | <id>]  list recorded runs or show one run's manifest\n"
                   "  /analyze <id>        extract observables from a run log (declared patterns)\n"
                   "  /compare <id>        compare extracted observables vs references; verdicts\n"
                   "                       CONSISTENT|CONTRARY|INCONCLUSIVE|NOT COMPARABLE\n"
                   "  /references [sub]    status|list|show <id>|cite <id> (corpus: references/)\n"
                   "  /manuscript          preview the provenance-tagged manuscript draft\n"
                   "  /publish             write paper/manuscript.md ([EDIT]); no fabrication\n"
                   "  /build               run the project build (G4_AGENT_BUILD_COMMAND or\n"
                   "                       detected CMake/GNUmake; requires PERMISSION=EDIT)\n"
                   "  /edit \"<change>\"      LLM-driven targeted project edit (requires EDIT)\n"
                   "  /ask <question>      bounded LLM tool loop (requires a configured backend)\n"
                   "  /exit, /quit         leave the session\n"
                   "\n"
                   "Project root: G4_AGENT_PROJECT_DIR (default: current directory).\n"
                   "Permission:  G4_AGENT_PERMISSION (default READ_ONLY; EDIT enables /build,\n"
                   "             /edit and project_write).\n"
                   "Exit status: 0 on success; 2 on a usage/unknown-command error.\n";
      return 0;
    }

    int HelpFor(const std::string& topic)
    {
      std::string name = topic;
      if (name[0] != '/') name = "/" + name;
      if (name == "/run")
        std::cout << "/run <macro-file>\n"
                     "  Executes the project application (G4_AGENT_RUN_COMMAND, or a command\n"
                     "  detected from the CMake/GNUmake project) in the project root\n"
                     "  with the confined macro appended as one argument, captures\n"
                     "  stdout/stderr, and writes runs/NNN/manifest.txt + log.txt.\n"
                     "  Requires\n"
                     "  G4_AGENT_PERMISSION=EDIT. The harness never runs the macro\n"
                     "  against its own embedded run manager.\n";
      else if (name == "/runs")
        std::cout << "/runs [list | <id>]\n"
                     "  Lists recorded runs under <project>/runs, or shows one run's\n"
                     "  manifest. Read-only.\n";
      else if (name == "/analyze")
        std::cout << "/analyze <run-id>\n"
                     "  Applies the observable patterns in validation/observables.txt to\n"
                     "  the run's captured log and reports the extracted numbers with\n"
                     "  their source lines. Unmatched patterns are NOT AVAILABLE, never\n"
                     "  guessed. Read-only.\n";
      else if (name == "/compare")
        std::cout << "/compare <run-id>\n"
                     "  Compares each extracted observable with validation/references.txt\n"
                     "  using a defined pull test. Verdicts: CONSISTENT (|pull| <= 2 sigma),\n"
                     "  CONTRARY, INCONCLUSIVE (missing/zero uncertainty), NOT COMPARABLE\n"
                     "  (unknown/incompatible units or missing value). This is a\n"
                     "  deterministic numeric comparison, not scientific acceptance.\n";
      else if (name == "/references")
        std::cout << "/references [status|list|show <id>|cite <id>]\n"
                     "  Interrogates the operator-supplied reference corpus under\n"
                     "  references/ (G4_AGENT_REFERENCES_DIR overrides). Citations are\n"
                     "  assembled from declared fields only; the core never fetches the\n"
                     "  network and never invents a citation. Read-only.\n";
      else if (name == "/manuscript")
        std::cout << "/manuscript\n"
                     "  Assembles the manuscript draft from persisted state (research\n"
                     "  notes, run manifests, the Stage-F comparison, the reference\n"
                     "  corpus). Every section is tagged by provenance; absent inputs are\n"
                     "  NOT AVAILABLE and never invented. Prints without writing.\n";
      else if (name == "/publish")
        std::cout << "/publish\n"
                     "  Writes the manuscript draft to paper/manuscript.md. Requires\n"
                     "  G4_AGENT_PERMISSION=EDIT. Results and citations come only from\n"
                     "  recorded runs and the reference corpus; none is fabricated.\n";
      else if (name == "/build")
        std::cout << "/build\n"
                     "  Runs G4_AGENT_BUILD_COMMAND, or a command detected from the\n"
                     "  CMake/GNUmake project, in the project root. The model/user\n"
                     "  contributes no arguments. Requires G4_AGENT_PERMISSION=EDIT. Output is\n"
                     "  capped and streamed.\n";
      else if (name == "/edit")
        std::cout << "/edit \"<change description>\"\n"
                     "  Delegates a targeted source change to the bounded agent loop;\n"
                     "  the change is applied only through the EDIT-gated project_write\n"
                     "  tool and is recorded in the project journal.\n";
      else if (name == "/ask")
        std::cout << "/ask <question>\n"
                     "  Drives the configured LLM backend through the bounded tool loop\n"
                     "  (G4_AGENT_MAX_STEPS). Without a configured backend it returns a\n"
                     "  deterministic 'LLM unavailable' report.\n";
      else if (name == "/validate")
        std::cout << "/validate [configuration|runtime]\n"
                     "  Composes the existing structural checks into one verdict. This\n"
                     "  is mechanical validation, never scientific validation.\n";
      else
        std::cout << "no detailed help for '" << topic << "'; see /help\n";
      return 0;
    }

    G4AgentManager& fAgent;
    bool fInteractive = false;
    bool fQuit = false;
};
}  // namespace

int main(int argc, char** argv)
{
  // Join argv[1..] into one command line; this yields the same command
  // semantics as the interactive session (one implementation, two modes).
  std::string commandLine;
  for (int i = 1; i < argc; ++i)
  {
    if (i > 1) commandLine += ' ';
    commandLine += argv[i];
  }

  const bool interactive = Trim(commandLine).empty();

  auto runManager = G4RunManagerFactory::CreateRunManager();

  int rc = 0;
  {
    // The agent must be destroyed before the run manager (it removes its UI
    // commands and releases services that reference the kernel).
    G4AgentManager agentManager;
    G4Cli cli(agentManager);

    if (interactive)
    {
      cli.Banner();
      cli.Repl();
    }
    else
    {
      rc = cli.RunLine(commandLine);
    }
  }

  delete runManager;
  return rc;
}