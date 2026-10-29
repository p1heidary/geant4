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

#include "G4AgentLLMConfig.hh"

#include "G4EnvironmentUtils.hh"
#include "globals.hh"

#include <cstdlib>

namespace
{
// Full-string environment read: G4GetEnv<G4String> would tokenize at
// whitespace, which can truncate model names or keys.
G4String EnvString(const char* name, const G4String& def)
{
  const char* value = std::getenv(name);
  return (value != nullptr) ? G4String(value) : def;
}
}  // namespace

const G4String G4AgentLLMConfig::kDefaultBaseUrl("https://api.openai.com/v1");

G4AgentLLMConfig G4AgentLLMConfig::FromEnvironment()
{
  G4AgentLLMConfig cfg;
  cfg.fProvider = EnvString("G4_AGENT_PROVIDER", "");
  cfg.fModel    = EnvString("G4_AGENT_MODEL", "");
  cfg.fBaseUrl  = EnvString("G4_AGENT_BASE_URL", kDefaultBaseUrl);
  cfg.fApiKey   = EnvString("G4_AGENT_API_KEY", "");
  cfg.fTimeoutSeconds = G4GetEnv<G4int>("G4_AGENT_TIMEOUT", 30);
  if (cfg.fTimeoutSeconds <= 0) cfg.fTimeoutSeconds = 30;
  cfg.fMaxSteps = G4GetEnv<G4int>("G4_AGENT_MAX_STEPS", 8);
  if (cfg.fMaxSteps <= 0) cfg.fMaxSteps = 8;
  cfg.fPermission = EnvString("G4_AGENT_PERMISSION", "READ_ONLY");
  return cfg;
}

G4String G4AgentLLMConfig::Describe() const
{
  if (!Available()) return "unavailable";
  if (!fModel.empty()) return fProvider + ":" + fModel;
  return fProvider;
}
