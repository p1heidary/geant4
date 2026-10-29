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
// This class registers the /agent/ UI command directory and its commands
// against the running G4UImanager, translating each command into a call
// on the shared G4Agent service layer. It contains no inspection logic
// itself. Commands are deliberately not broadcast to worker threads;
// they should be inspected from the master thread.
//
// All reports are deterministic, human-readable text prefixed with
// [G4AGENT] and are produced exclusively from existing public Geant4
// interfaces.

#ifndef G4AGENTMESSENGER_HH
#define G4AGENTMESSENGER_HH

#include "G4UImessenger.hh"
#include "globals.hh"

class G4Agent;
class G4UIdirectory;
class G4UIcmdWithoutParameter;
class G4UIcmdWithAString;

class G4AgentMessenger : public G4UImessenger
{
  public:

    explicit G4AgentMessenger(G4Agent& agent);
    ~G4AgentMessenger() override;

    void SetNewValue(G4UIcommand*, G4String) override;

  private:

    G4Agent& fAgent;

    G4UIdirectory* fAgentDirectory = nullptr;

    G4UIcmdWithoutParameter* fStatusCmd         = nullptr;
    G4UIcmdWithoutParameter* fVersionCmd        = nullptr;
    G4UIcmdWithoutParameter* fCapabilitiesCmd   = nullptr;
    G4UIcmdWithAString*      fListUICmd         = nullptr;
    G4UIcmdWithoutParameter* fListParticlesCmd  = nullptr;
    G4UIcmdWithoutParameter* fListProcessesCmd  = nullptr;
    G4UIcmdWithoutParameter* fListMaterialsCmd  = nullptr;
    G4UIcmdWithoutParameter* fListVolumesCmd    = nullptr;
    G4UIcmdWithoutParameter* fInspectAppCmd     = nullptr;
    G4UIcmdWithAString*      fInspectGeomCmd    = nullptr;
    G4UIcmdWithoutParameter* fInspectPhysCmd    = nullptr;
    G4UIcmdWithoutParameter* fInspectRunCmd     = nullptr;
    G4UIcmdWithoutParameter* fInspectHistoryCmd = nullptr;
    G4UIcmdWithoutParameter* fInspectTrackCmd   = nullptr;
    G4UIcmdWithAString*      fCheckGeomCmd      = nullptr;
    G4UIcmdWithoutParameter* fCheckPhysCmd      = nullptr;
    G4UIcmdWithoutParameter* fCheckRunCmd       = nullptr;
    G4UIcmdWithoutParameter* fReportCmd         = nullptr;
    G4UIcmdWithAString*      fValidateCmd       = nullptr;
    G4UIcmdWithAString*      fKnowledgeCmd      = nullptr;
    G4UIcmdWithoutParameter* fLLMCmd            = nullptr;
    G4UIcmdWithAString*      fAskCmd            = nullptr;
};

#endif
