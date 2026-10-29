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

#include "G4AgentManager.hh"

#include "G4Agent.hh"
#include "G4AgentLLMConfig.hh"
#include "G4AgentMessenger.hh"
#include "G4AgentPolicy.hh"
#include "G4Version.hh"

#include <vector>

G4AgentManager::G4AgentManager()
{
  fAgent = new G4Agent;
  fMessenger = new G4AgentMessenger(*fAgent);
}

G4AgentManager::~G4AgentManager()
{
  delete fMessenger;
  delete fAgent;
}

const G4String& G4AgentManager::GetInterfaceVersion()
{
  static const G4String interfaceVersion("0.12.0");
  return interfaceVersion;
}

G4String G4AgentManager::Ask(const G4String& question)
{
  return fAgent != nullptr ? fAgent->Ask(question) : G4String("LLM unavailable");
}

G4String G4AgentManager::Validate(const G4String& tier)
{
  return fAgent != nullptr ? fAgent->Validate(tier) : G4String("agent not active");
}

const std::vector<G4String>& G4AgentManager::ToolNames()
{
  return G4Agent::ToolNames();
}

G4String G4AgentManager::ToolDescription(const G4String& name)
{
  return G4Agent::ToolDescription(name);
}

G4String G4AgentManager::PermissionName()
{
  const auto config = G4AgentLLMConfig::FromEnvironment();
  return G4AgentPolicy::FromString(config.Permission()).Name();
}

G4String G4AgentManager::ExecuteTool(const G4String& name, const G4String& argument)
{
  if (fAgent == nullptr)
  {
    return G4String("[G4AGENT] tool error\nagent not active");
  }

  const auto config = G4AgentLLMConfig::FromEnvironment();
  const G4AgentPolicy policy = G4AgentPolicy::FromString(config.Permission());
  const G4String refusal =
    policy.CheckTool(name, G4Agent::ToolIsReadOnly(name), G4Agent::ToolRequiresEdit(name));
  if (!refusal.empty())
  {
    G4String report = "[G4AGENT] tool refused\nreason     : ";
    report += refusal;
    report += "\npermission : " + policy.Name();
    return report;
  }
  return fAgent->ExecuteTool(name, argument);
}

const G4String& G4AgentManager::GetGeant4Version()
{
  static const G4String geant4Version(G4Version);
  return geant4Version;
}
