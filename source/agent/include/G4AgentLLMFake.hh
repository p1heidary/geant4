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
// G4AgentLLMFake
//
// Class description
//
// Deterministic script-driven backend used for tests and for offline
// exercises of the reasoning loop. Script steps are supplied
// through the G4_AGENT_FAKE_SCRIPT environment variable, separated by
// '|', or programmatically via SetScript:
//
//   tool <name> [argument]   -> assistant requests a semantic tool call
//   final <text...>          -> assistant returns a final text answer
//   fail                     -> deterministic scripted backend failure
//
// Each Complete() call consumes one step; when the script is exhausted a
// final "no further steps" reply follows so tests remain deterministic.
// G4_AGENT_FAKE_REPEAT replicates the parsed script N times (used to drive
// the loop to its step bound). No network access ever.

#ifndef G4AGENTLLMFAKE_HH
#define G4AGENTLLMFAKE_HH

#include "G4AgentLLMBackend.hh"
#include "G4String.hh"

#include <vector>

class G4AgentLLMFake : public G4AgentLLMBackend
{
  public:

    G4AgentLLMFake();

    G4String Name() const override { return "fake"; }

    G4LLMReply Complete(const std::vector<G4LLMMessage>& messages,
                        const std::vector<G4LLMToolSpec>& tools) override;

    void SetScript(const std::vector<G4String>& lines);
    G4int ScriptStep() const { return fStep; }

  private:

    std::vector<G4String> fScript;
    G4int fStep = 0;
};

#endif
