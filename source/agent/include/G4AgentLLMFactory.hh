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
// G4AgentLLMFactory
//
// Class description
//
// Instantiates a backend from the environment configuration.
// The always-compiled fake is available without any build option; real
// HTTP providers are supplied through a weak symbol resolved at link time
// when the optional G4agent_llm module is present. Absence of a provider
// configuration is normal: Create() returns nullptr and the agent reports
// LLM unavailable.


#ifndef G4AGENTLLMFACTORY_HH
#define G4AGENTLLMFACTORY_HH

#include "G4AgentLLMBackend.hh"
#include "G4AgentLLMConfig.hh"

#include <memory>

// Instantiates a backend for the configured provider. The always-compiled
// fake needs no build option; HTTP providers are resolved at run time from
// the optional G4agent_llm module (extern "C" G4AgentLLMCreateHTTP) via
// dlsym. Returns nullptr (and reports) when no backend is available.
class G4AgentLLMFactory
{
  public:

    static std::unique_ptr<G4AgentLLMBackend> Create(const G4AgentLLMConfig& config);
};

#endif
