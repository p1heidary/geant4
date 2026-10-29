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
// G4AgentLLMConfig
//
// Class description
//
// Environment-driven LLM configuration (g4.txt 12). Reads:
//
//   G4_AGENT_PROVIDER    "fake" | "openai" (unset = LLM unavailable, valid)
//   G4_AGENT_MODEL       model name
//   G4_AGENT_BASE_URL    OpenAI-compatible endpoint base (default api.openai.com)
//   G4_AGENT_API_KEY     bearer token; NEVER printed or exposed to the model
//   G4_AGENT_TIMEOUT     request timeout seconds (default 30)
//   G4_AGENT_MAX_STEPS   reasoning-loop model-step limit (default 8)
//   G4_AGENT_PERMISSION  READ_ONLY (default) | SUGGEST | EDIT | AUTONOMOUS;
//                        higher levels grant nothing today (no write tools)
//   G4_AGENT_PROJECT_DIR project root for the read-only project tools
//                        (default: process working directory)
#ifndef G4AGENTLLMCONFIG_HH
#define G4AGENTLLMCONFIG_HH

#include "G4String.hh"

class G4AgentLLMConfig
{
  public:

    static G4AgentLLMConfig FromEnvironment();

    const G4String& Provider() const { return fProvider; }
    const G4String& Model() const { return fModel; }
    const G4String& BaseUrl() const { return fBaseUrl; }
    const G4String& ApiKey() const { return fApiKey; }
    G4int TimeoutSeconds() const { return fTimeoutSeconds; }
    G4int MaxSteps() const { return fMaxSteps; }
    const G4String& Permission() const { return fPermission; }

    // A configured provider name makes the LLM layer available.
    G4bool Available() const { return !fProvider.empty(); }

    // Human-readable state for reports: "unavailable", "fake",
    // "openai:<model>". Does not include the API key.
    G4String Describe() const;

    static const G4String kDefaultBaseUrl;

  private:

    G4String fProvider;
    G4String fModel;
    G4String fBaseUrl;
    G4String fApiKey;
    G4int fTimeoutSeconds = 30;
    G4int fMaxSteps = 8;
    G4String fPermission;
};

#endif
