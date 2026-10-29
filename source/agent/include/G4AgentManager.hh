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
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// G4AgentManager
//
// Class description
//
// This class is the explicit, optional entry point of the Geant4-native
// agent interface. Constructing a G4AgentManager object activates the
// interface by installing the /agent/ UI command directory and its
// inspection, listing and diagnostic commands in the running G4UImanager.
//
// The interface is deterministic and agent-oriented. It only reports facts,
// configuration and state through existing public Geant4 interfaces; it does
// not contain AI or network functionality.
//
// An application that never constructs a G4AgentManager is completely
// unaffected by the agent module.

#ifndef G4AGENTMANAGER_HH
#define G4AGENTMANAGER_HH

#include "G4String.hh"

#include <vector>

class G4Agent;
class G4AgentMessenger;

class G4AgentManager
{
  public:

    G4AgentManager();
    ~G4AgentManager();

    // deleted copy semantics
    G4AgentManager(const G4AgentManager&) = delete;
    G4AgentManager& operator=(const G4AgentManager&) = delete;

    // Version of the agent interface provided by this build.
    static const G4String& GetInterfaceVersion();

    // Geant4 version as reported by the toolkit at run time.
    static const G4String& GetGeant4Version();

    // Forwarding entry points for the shared native services (no second
    // implementation lives here or in any CLI): run one bounded tool-loop
    // question, and list the registered semantic tools with descriptions.
    G4String Ask(const G4String& question);
    G4String Validate(const G4String& tier);
    static const std::vector<G4String>& ToolNames();
    static G4String ToolDescription(const G4String& name);

    // Execute a registered semantic tool under the configured permission
    // policy (G4_AGENT_PERMISSION). Read-only tools are always allowed;
    // modifying tools (project_write, project_build) require EDIT or
    // AUTONOMOUS and are otherwise refused fail-closed. Returns the tool's
    // deterministic [G4AGENT] result, or a refusal report. Used by the g4
    // CLI; the reasoning loop uses the same gate through G4AgentPolicy.
    G4String ExecuteTool(const G4String& name, const G4String& argument);

    // Configured permission level name (READ_ONLY|SUGGEST|EDIT|AUTONOMOUS).
    static G4String PermissionName();

  private:

    G4Agent* fAgent = nullptr;
    G4AgentMessenger* fMessenger = nullptr;
};

#endif
