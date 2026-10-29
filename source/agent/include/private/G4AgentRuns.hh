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
// *                                                                  *
// ********************************************************************
//
// G4AgentRuns
//
// Class description
//
// Deterministic project execution and run records (reproducibility layer).
// The harness never runs a simulation against its own embedded, empty run
// manager: it executes the *project's* application through an
// operator-configured or project-detected command (G4_AGENT_RUN_COMMAND,
// mirroring G4_AGENT_BUILD_COMMAND), with a path-confined macro, in the
// project root.
// Each execution produces a self-contained record under <root>/runs/NNN/:
//
//   manifest.txt   deterministic label : value fields
//   log.txt        captured combined stdout/stderr (size-capped)
//
// Missing information is recorded as "unknown"; observables are never
// fabricated -- extracting physics results is the job of external analysis.
// Reads (list/show) are strictly read-only and path-confined; only RunProject
// writes, and it is reached solely through the EDIT-gated run_project tool.

#ifndef G4AGENTRUNS_HH
#define G4AGENTRUNS_HH

#include "G4String.hh"

class G4AgentProject;

class G4AgentRuns
{
  public:

    explicit G4AgentRuns(const G4AgentProject& project);

    // Execute the project application with an optional confined macro and
    // write a run record. Returns a deterministic [G4AGENT] report.
    G4String RunProject(const G4String& macro) const;

    // Read-only views over the recorded runs under <root>/runs.
    G4String ListRuns() const;
    G4String ShowRun(const G4String& id) const;

  private:

    const G4AgentProject& fProject;
};

#endif