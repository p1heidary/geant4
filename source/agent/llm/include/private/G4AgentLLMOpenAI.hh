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
// G4AgentLLMOpenAI
//
// Class description
//
// OpenAI-compatible chat-completions backend: the single first
// provider, isolated behind G4AgentLLMBackend. Works against hosted and
// local OpenAI-compatible endpoints via G4_AGENT_BASE_URL. Lives in the 
// optional G4agent_llm module; the core library contains no network code.
//
// Tool calls use the standard "function" tool schema with a single string
// argument convention shared with the agent tool layer.

#ifndef G4AGENTLLMOPENAI_HH
#define G4AGENTLLMOPENAI_HH

#include "G4AgentLLMBackend.hh"
#include "G4AgentLLMConfig.hh"

#include <memory>

class G4AgentLLMOpenAI : public G4AgentLLMBackend
{
  public:

    explicit G4AgentLLMOpenAI(const G4AgentLLMConfig& config);

    G4String Name() const override { return "openai"; }

    G4LLMReply Complete(const std::vector<G4LLMMessage>& messages,
                        const std::vector<G4LLMToolSpec>& tools) override;

  private:

    G4String fUrl;
    G4String fApiKey;
    G4String fModel;
    G4int fTimeoutSeconds;
};

#endif
