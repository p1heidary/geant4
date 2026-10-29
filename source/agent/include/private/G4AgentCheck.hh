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
// G4AgentCheck
//
// Class description
//
// Structural diagnostic service: answers "is an observable configuration
// internally sane according to this limited check?" and nothing more.
// Checks reuse existing Geant4 machinery (the overlap engine, the
// state manager, process/particle registries); no second validation
// engine is implemented.
//
// Results use PASS / WARNING / FAIL / NOT AVAILABLE only where those
// semantics are technically justified. Structural success is never
// presented as scientific validation.

#ifndef G4AGENTCHECK_HH
#define G4AGENTCHECK_HH

#include "G4String.hh"

class G4AgentCheck
{
  public:

    G4AgentCheck() = default;

    // scope: "world" or a physical volume name
    G4String GeometryCheck(const G4String& scope) const;
    G4String PhysicsCheck() const;
    G4String RunCheck() const;
};

#endif
