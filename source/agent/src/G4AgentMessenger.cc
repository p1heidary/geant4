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
// G4AgentMessenger
//
// Class description
//
// Registers the /agent/ UI command directory and its commands against the
// running G4UImanager. The messenger is deliberately thin: it translates
// UI commands into G4Agent semantic tool calls and streams the returned
// deterministic text reports. All inspection and check logic lives in the
// shared service layer, so UI commands and future LLM tool dispatch use
// the same implementations.
//
// Commands are deliberately not broadcast to worker threads; they should
// be inspected from the master thread.

#include "G4AgentMessenger.hh"

#include "G4Agent.hh"
#include "G4AgentManager.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcommand.hh"
#include "G4UIdirectory.hh"
#include "G4UImanager.hh"
#include "G4ios.hh"

G4AgentMessenger::G4AgentMessenger(G4Agent& agent) : fAgent(agent)
{
  fAgentDirectory = new G4UIdirectory("/agent/", false);
  fAgentDirectory->SetGuidance("Geant4-native agent inspection interface.");

  fStatusCmd = new G4UIcmdWithoutParameter("/agent/status", this);
  fStatusCmd->SetGuidance("Report global runtime status.");

  fVersionCmd = new G4UIcmdWithoutParameter("/agent/version", this);
  fVersionCmd->SetGuidance("Report Geant4 and agent interface versions.");

  fCapabilitiesCmd = new G4UIcmdWithoutParameter("/agent/capabilities", this);
  fCapabilitiesCmd->SetGuidance("Report the capabilities of this agent build.");

  fListUICmd = new G4UIcmdWithAString("/agent/list/ui", this);
  fListUICmd->SetGuidance(
    "List the UI command hierarchy. An optional directory path"
    " scopes the listing.");
  fListUICmd->SetParameterName("directory", true);
  fListUICmd->SetDefaultValue("/");

  fListParticlesCmd = new G4UIcmdWithoutParameter("/agent/list/particles", this);
  fListParticlesCmd->SetGuidance("List registered particle types.");

  fListProcessesCmd = new G4UIcmdWithoutParameter("/agent/list/processes", this);
  fListProcessesCmd->SetGuidance("List registered process names.");

  fListMaterialsCmd = new G4UIcmdWithoutParameter("/agent/list/materials", this);
  fListMaterialsCmd->SetGuidance("List registered materials.");

  fListVolumesCmd = new G4UIcmdWithoutParameter("/agent/list/volumes", this);
  fListVolumesCmd->SetGuidance("List registered physical volumes.");

  fInspectAppCmd = new G4UIcmdWithoutParameter("/agent/inspect/application", this);
  fInspectAppCmd->SetGuidance("Report application lifecycle and components.");

  fInspectGeomCmd = new G4UIcmdWithAString("/agent/inspect/geometry", this);
  fInspectGeomCmd->SetGuidance(
    "Report geometry information. Scope may be 'summary',"
    " 'world', or a volume name.");
  fInspectGeomCmd->SetParameterName("scope", true);
  fInspectGeomCmd->SetDefaultValue("summary");

  fInspectPhysCmd = new G4UIcmdWithoutParameter("/agent/inspect/physics", this);
  fInspectPhysCmd->SetGuidance("Report the current physics configuration.");

  fInspectRunCmd = new G4UIcmdWithoutParameter("/agent/inspect/run", this);
  fInspectRunCmd->SetGuidance("Report run-manager and run state.");

  fInspectHistoryCmd = new G4UIcmdWithoutParameter("/agent/inspect/history", this);
  fInspectHistoryCmd->SetGuidance("Report commands recorded in the UI command history.");

  fInspectTrackCmd = new G4UIcmdWithoutParameter("/agent/inspect/tracking", this);
  fInspectTrackCmd->SetGuidance("Report tracking availability.");

  fCheckGeomCmd = new G4UIcmdWithAString("/agent/check/geometry", this);
  fCheckGeomCmd->SetGuidance(
    "Run the existing geometry overlap check and summarize."
    " Scope may be 'world' or a volume name.");
  fCheckGeomCmd->SetParameterName("scope", true);
  fCheckGeomCmd->SetDefaultValue("world");

  fCheckPhysCmd = new G4UIcmdWithoutParameter("/agent/check/physics", this);
  fCheckPhysCmd->SetGuidance("Structural sanity check of the physics configuration.");

  fCheckRunCmd = new G4UIcmdWithoutParameter("/agent/check/run", this);
  fCheckRunCmd->SetGuidance("Structural sanity check of the run-manager state.");

  fReportCmd = new G4UIcmdWithoutParameter("/agent/report", this);
  fReportCmd->SetGuidance("Report an aggregated summary of the application.");

  fLLMCmd = new G4UIcmdWithoutParameter("/agent/llm", this);
  fLLMCmd->SetGuidance("Report LLM layer configuration state (never the API key).");

  fValidateCmd = new G4UIcmdWithAString("/agent/validate", this);
  fValidateCmd->SetGuidance(
    "Validation verdict over existing checks (mechanical,"
    " never scientific). Argument: configuration | runtime |"
    " empty for both.");
  fValidateCmd->SetParameterName("tier", true);
  fValidateCmd->SetDefaultValue("");

  fKnowledgeCmd = new G4UIcmdWithAString("/agent/knowledge", this);
  fKnowledgeCmd->SetGuidance(
    "Local optional knowledge corpus (no network). Argument:"
    " status | catalog | search <text> | gap <text>.");
  fKnowledgeCmd->SetParameterName("subcommand", true);
  fKnowledgeCmd->SetDefaultValue("status");

  fAskCmd = new G4UIcmdWithAString("/agent/ask", this);
  fAskCmd->SetGuidance(
    "Question answered by the configured LLM backend through the bounded"
    " read-only tool loop. Deterministic 'LLM unavailable' when unconfigured.");
  fAskCmd->SetParameterName("question", false);

  G4UIcommand* commands[] = {
    fStatusCmd,        fVersionCmd,       fCapabilitiesCmd,   fListUICmd,       fListParticlesCmd,
    fListProcessesCmd, fListMaterialsCmd, fListVolumesCmd,    fInspectAppCmd,   fInspectGeomCmd,
    fInspectPhysCmd,   fInspectRunCmd,    fInspectHistoryCmd, fInspectTrackCmd, fCheckGeomCmd,
    fCheckPhysCmd,     fCheckRunCmd,      fReportCmd,         fLLMCmd,          fAskCmd,
    fValidateCmd,      fKnowledgeCmd};
  for (G4UIcommand* command : commands)
    command->SetToBeBroadcasted(false);
}

G4AgentMessenger::~G4AgentMessenger()
{
  delete fStatusCmd;
  delete fVersionCmd;
  delete fCapabilitiesCmd;
  delete fListUICmd;
  delete fListParticlesCmd;
  delete fListProcessesCmd;
  delete fListMaterialsCmd;
  delete fListVolumesCmd;
  delete fInspectAppCmd;
  delete fInspectGeomCmd;
  delete fInspectPhysCmd;
  delete fInspectRunCmd;
  delete fInspectHistoryCmd;
  delete fInspectTrackCmd;
  delete fCheckGeomCmd;
  delete fCheckPhysCmd;
  delete fCheckRunCmd;
  delete fReportCmd;
  delete fValidateCmd;
  delete fKnowledgeCmd;
  delete fLLMCmd;
  delete fAskCmd;
  delete fAgentDirectory;
}

void G4AgentMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  // UI commands translate into shared agent services; output formatting and
  // Geant4 observation are owned by those services, not by this messenger.
  if (command == fStatusCmd)
    G4cout << fAgent.Status() << G4endl;
  else if (command == fVersionCmd)
    G4cout << fAgent.Version() << G4endl;
  else if (command == fCapabilitiesCmd)
    G4cout << fAgent.Capabilities() << G4endl;
  else if (command == fListUICmd)
    fAgent.ListUICommands(newValue);
  else if (command == fListParticlesCmd)
    G4cout << fAgent.ListParticles() << G4endl;
  else if (command == fListProcessesCmd)
    G4cout << fAgent.ListProcesses() << G4endl;
  else if (command == fListMaterialsCmd)
    G4cout << fAgent.ListMaterials() << G4endl;
  else if (command == fListVolumesCmd)
    G4cout << fAgent.ListVolumes() << G4endl;
  else if (command == fInspectAppCmd)
    G4cout << fAgent.InspectApplication() << G4endl;
  else if (command == fInspectGeomCmd)
    G4cout << fAgent.InspectGeometry(newValue) << G4endl;
  else if (command == fInspectPhysCmd)
    G4cout << fAgent.InspectPhysics() << G4endl;
  else if (command == fInspectRunCmd)
    G4cout << fAgent.InspectRun() << G4endl;
  else if (command == fInspectHistoryCmd)
    G4cout << fAgent.InspectHistory() << G4endl;
  else if (command == fInspectTrackCmd)
    G4cout << fAgent.InspectTracking() << G4endl;
  else if (command == fCheckGeomCmd)
    G4cout << fAgent.CheckGeometry(newValue) << G4endl;
  else if (command == fCheckPhysCmd)
    G4cout << fAgent.CheckPhysics() << G4endl;
  else if (command == fCheckRunCmd)
    G4cout << fAgent.CheckRun() << G4endl;
  else if (command == fReportCmd)
    G4cout << fAgent.Report() << G4endl;
  else if (command == fValidateCmd)
    G4cout << fAgent.Validate(newValue) << G4endl;
  else if (command == fKnowledgeCmd)
  {
    const auto sp = newValue.find(' ');
    const G4String sub = (sp == G4String::npos) ? newValue : newValue.substr(0, sp);
    const G4String arg = (sp == G4String::npos) ? G4String("") : newValue.substr(sp + 1);
    G4cout << fAgent.Knowledge(sub, arg) << G4endl;
  }
  else if (command == fLLMCmd)
    G4cout << fAgent.LLMStatus() << G4endl;
  else if (command == fAskCmd)
    G4cout << fAgent.Ask(newValue) << G4endl;
}
