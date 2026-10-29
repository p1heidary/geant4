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
// G4AgentInspect
//
// Class description
//
// Geant4-aware read-only observation service. Every operation answers
// "what exists / what is configured / what is the current runtime state?"
// using public Geant4 interfaces, and returns a deterministic text block
// without printing. The UI messenger and the future LLM tool layer call
// the same implementations (ARCHITECTURE.md, Stage 3).
//
// Information that cannot be reliably determined is reported as
// NOT AVAILABLE within the text; nothing here modifies Geant4 state.

#ifndef G4AGENTINSPECT_HH
#define G4AGENTINSPECT_HH

#include "G4String.hh"

class G4AgentInspect
{
  public:

    G4AgentInspect() = default;

    // Blocks without the [G4AGENT] header line; suitable for embedding
    // in the aggregated report.
    G4String StatusFields() const;
    G4String VersionBlock() const;
    G4String CapabilitiesBlock() const;

    // UI discovery delegates to G4UImanager::ListCommands(), which emits
    // the command hierarchy itself; printing here is intentional reuse
    // rather than capturing and reformatting existing facility output.
    void PrintUIList(const G4String& directory) const;

    G4String ParticleListBlock() const;
    G4String ProcessListBlock() const;
    G4String MaterialListBlock() const;
    G4String VolumeListBlock() const;
    G4String ApplicationFields() const;
    G4String GeometryBlock(const G4String& scope) const;
    G4String GeometryFields() const;
    G4String PhysicsFields() const;
    G4String RunFields() const;
    G4String HistoryBlock() const;
    G4String TrackingBlock() const;
};

#endif
