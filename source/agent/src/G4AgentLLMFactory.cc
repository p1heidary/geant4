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
// G4AgentLLMFactory implementation
//
// The optional HTTP backend (module G4agent_llm) provides the C-linkage
// symbol G4AgentLLMCreateHTTP. The core library never links against that
// module: it resolves the symbol at run time with dlsym(RTLD_DEFAULT). If
// the application (or toolkit build) also links G4agent_llm, the provider
// works; otherwise the agent reports LLM unavailable without any network
// capability present in the process. This keeps the default build free of
// network code and of link-order fragility.
//

#include "G4AgentLLMFactory.hh"

#include "G4AgentLLMFake.hh"
#include "G4Exception.hh"

#if __has_include(<dlfcn.h>)
#  include <dlfcn.h>
#  define G4AGENT_HAVE_DLSYM 1
#endif

std::unique_ptr<G4AgentLLMBackend> G4AgentLLMFactory::Create(const G4AgentLLMConfig& config)
{
  if (!config.Available()) return nullptr;

  if (config.Provider() == "fake") return std::make_unique<G4AgentLLMFake>();

#ifdef G4AGENT_HAVE_DLSYM
  using CreateFn = G4AgentLLMBackend* (*)(const G4AgentLLMConfig&);
  auto symbol = dlsym(RTLD_DEFAULT, "G4AgentLLMCreateHTTP");
  if (symbol != nullptr)
  {
    auto create = reinterpret_cast<CreateFn>(symbol);
    auto* backend = create(config);
    if (backend != nullptr) return std::unique_ptr<G4AgentLLMBackend>(backend);
  }
#endif

  G4ExceptionDescription description;
  description << "LLM provider '" << config.Provider()
              << "' requires the optional G4agent_llm HTTP backend module; "
              << "LLM unavailable.";
  G4Exception("G4AgentLLMFactory::Create", "G4AgentLLM0001", JustWarning, description);
  return nullptr;
}
