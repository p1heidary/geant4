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
// G4Agent
//
// Class description
//
// The Geant4-native agent services object: the single implementation point
// of the semantic tool layer. UI commands in G4AgentMessenger and future
// LLM tool dispatch both call these methods;
// neither re-implements observation or checking.
//
// Every operation is read-only with respect to Geant4 state and returns a
// deterministic, human-readable text block (with its [G4AGENT] header line)
// or prints directly where an existing Geant4 facility already provides the
// output (UI command listing). No operation requires an LLM.

#ifndef G4AGENT_HH
#define G4AGENT_HH

#include "G4AgentAnalysis.hh"
#include "G4AgentCheck.hh"
#include "G4AgentInspect.hh"
#include "G4AgentKnowledge.hh"
#include "G4AgentProject.hh"
#include "G4AgentPublication.hh"
#include "G4AgentRuns.hh"
#include "G4AgentValidate.hh"
#include "G4String.hh"

#include <vector>

class G4Agent
{
  public:

    G4Agent();

    // Semantic tools (names mirror the planned LLM tool surface)
    G4String Status() const;
    G4String Version() const;
    G4String Capabilities() const;
    void ListUICommands(const G4String& directory) const;
    G4String ListParticles() const;
    G4String ListProcesses() const;
    G4String ListMaterials() const;
    G4String ListVolumes() const;
    G4String InspectApplication() const;
    G4String InspectGeometry(const G4String& scope) const;
    G4String InspectPhysics() const;
    G4String InspectRun() const;
    G4String InspectHistory() const;
    G4String InspectTracking() const;
    G4String CheckGeometry(const G4String& scope) const;
    G4String CheckPhysics() const;
    G4String CheckRun() const;
    G4String Report() const;

    // Validation orchestration over the existing checks:
    // composes PASS/WARNING/FAIL/NOT AVAILABLE/INSUFFICIENT EVIDENCE; never
    // scientific validation.
    G4String Validate(const G4String& tier) const;

    // Local optional knowledge retrieval: status|catalog|search|gap
    G4String Knowledge(const G4String& subcommand, const G4String& argument) const;

    // Semantic tool registry: the exact tool names exposed to the LLM.
    // Every registered tool is read-only.
    static const std::vector<G4String>& ToolNames();
    static G4String ToolDescription(const G4String& name);
    static G4bool IsKnownTool(const G4String& name);

    // Registry permission classification, used by the shared policy gate in
    // both the reasoning loop and the g4 CLI. Unknown names report false for
    // both, which the policy treats as fail-closed (refused).
    static G4bool ToolIsReadOnly(const G4String& name);
    static G4bool ToolRequiresEdit(const G4String& name);

    // Execute a registered, read-only semantic tool by name with the UI
    // command's argument convention. Returns the [G4AGENT] report text.
    // Unknown names return a deterministic error block; this method does
    // not itself consult the policy (the Ask loop gates first).
    G4String ExecuteTool(const G4String& name, const G4String& argument) const;

    // Controlled simulation: run a macro confined to the project root via
    // the existing UI manager (journal-recorded by the loop).
    G4String RunMacroFile(const G4String& macroPath) const;
    G4String RunProjectBuild() const;

    // Project execution and reproducibility records: execute the
    // project application (G4_AGENT_RUN_COMMAND or a detected target) with a
    // confined macro and record a run under <root>/runs/NNN/; list/show runs.
    G4String RunProject(const G4String& macro) const;
    G4String ListRuns() const;
    G4String ShowRun(const G4String& id) const;

    // Deterministic scientific-analysis / validation foundation:
    // extract observables from a run log and compare against references with
    // the CONSISTENT/CONTRARY/INCONCLUSIVE/NOT COMPARABLE vocabulary.
    G4String Analysis(const G4String& subcommand, const G4String& argument) const;

    // Literature corpus and provenance-tagged publication.
    G4String Bibliography(const G4String& subcommand, const G4String& argument) const;
    G4String Manuscript() const;
    G4String Publish() const;

    // LLM layer (optional consumer; these never require a configured LLM).
    // Ask runs the bounded read-only tool loop (observe -> tool -> result
    // -> reason -> final answer) when a backend is configured; unconfigured
    // state is a deterministic report, never a crash or a hang.
    G4String LLMStatus() const;
    G4String Ask(const G4String& question) const;

    // Section renderers shared with Report() (no [G4AGENT] header)
    G4String StatusFields() const;
    G4String ApplicationFields() const;
    G4String PhysicsFields() const;
    G4String GeometryFields() const;
    G4String RunFields() const;
    G4String HistoryBlock() const;

  private:

    G4AgentInspect fInspect;
    G4AgentCheck fCheck;
    G4AgentValidate fValidate;
    G4AgentProject fProject;
    G4AgentKnowledge fKnowledge;
    G4AgentRuns fRuns;
    G4AgentAnalysis fAnalysis;
    G4AgentPublication fPublication;
};

#endif
