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
// G4AgentValidate
//
// Class description
//
// Validation-orchestration service. It does NOT implement any check itself:
// it runs the existing structural checks (G4AgentCheck, which reuse the 
// Geant4 overlap engine, state manager and registries) and composes their 
// verdicts into a single validation record with the agreed vocabulary:
//
//   PASS / WARNING / FAIL / NOT AVAILABLE / INSUFFICIENT EVIDENCE
//
// INSUFFICIENT EVIDENCE means "the native evidence available in the current
// application state cannot support any verdict" (e.g. nothing initialized
// yet, or no completed run to observe). The verdict is a statement about
// that specific, limited mechanical evidence; it is never scientific
// validation, and the report says so in every state.
//
// Tiers implemented natively: configuration (are the pieces present and
// sane?) and runtime (did a completed run actually observe events?).
// Numerical/statistical/reference comparison requires run output outside
// this process; the service reports what exists and marks the rest
// INSUFFICIENT EVIDENCE for the external-analysis layer.

#ifndef G4AGENTVALIDATE_HH
#define G4AGENTVALIDATE_HH

#include "G4AgentCheck.hh"
#include "G4AgentInspect.hh"
#include "G4String.hh"

class G4AgentValidate
{
  public:

    explicit G4AgentValidate(const G4AgentCheck& checks, const G4AgentInspect& inspect);

    // tier: "configuration" | "runtime" | empty for both
    G4String Validate(const G4String& tier) const;

  private:

    const G4AgentCheck& fChecks;
    const G4AgentInspect& fInspect;
};

#endif
