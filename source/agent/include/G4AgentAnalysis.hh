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
// G4AgentAnalysis
//
// Class description
//
// Deterministic scientific-analysis and validation foundation.
// It turns recorded runs into extracted observables and compares them with
// operator-supplied references using only actual numbers and defined
// calculations. Numerical judgment is code, not an LLM opinion.
//
// Two layers:
//
//  1. Pure, unit-testable helpers (no I/O): measurement parsing, unit
//     normalization over a small explicit dimension table, and a statistical
//     comparison that returns one of the four agreed verdicts:
//
//        CONSISTENT | CONTRARY | INCONCLUSIVE | NOT COMPARABLE
//
//     Rules (all deterministic):
//       * unknown/incompatible units        -> NOT COMPARABLE
//       * missing value on either side       -> NOT COMPARABLE
//       * compatible units but missing/zero  -> INCONCLUSIVE
//         uncertainty on either side
//       * otherwise pull = (sim-ref)/sqrt(sigma_sim^2+sigma_ref^2):
//           |pull| <= threshold -> CONSISTENT, else CONTRARY
//
//  2. Project-local orchestration (reusing G4AgentProject for confinement):
//     `analyze_run` extracts observables from a run's captured log using
//     operator-declared patterns (validation/observables.txt), and
//     `compare_run` compares them against validation/references.txt.
//
// The service never invents values: an unmatched pattern, an unknown unit or
// a missing uncertainty is reported as such, never guessed. A CONSISTENT
// verdict states only that a numeric comparison matched within uncertainties;
// it is not, by itself, scientific acceptance.

#ifndef G4AGENTANALYSIS_HH
#define G4AGENTANALYSIS_HH

#include "G4AgentProject.hh"
#include "G4String.hh"

#include <vector>

struct G4AgentMeasurement
{
    G4bool valid = false;  // a numeric value was parsed
    double value = 0.0;
    double uncertainty = 0.0;
    G4bool hasUncertainty = false;  // uncertainty present and > 0
    G4String unit;  // as declared
    G4String canonicalUnit;  // after normalization ("" if unknown)
    G4bool unitKnown = false;
    double toCanonical = 1.0;  // multiply value by this for canonical
};

enum class G4AgentVerdict
{
  Consistent,
  Contrary,
  Inconclusive,
  NotComparable
};

class G4AgentAnalysis
{
  public:

    explicit G4AgentAnalysis(const G4AgentProject& project);

    // subcommand: analyze <id> | compare <id> | status
    G4String Run(const G4String& subcommand, const G4String& argument) const;

    // ---- pure helpers (no I/O; unit-tested) -------------------------------

    // Parse "value" / "uncertainty" / "unit" textual fields into a
    // measurement. Empty value -> invalid; uncertainty <= 0 -> no uncertainty.
    static G4AgentMeasurement Measurement(const G4String& value, const G4String& uncertainty,
                                          const G4String& unit);

    // Normalize a unit over the explicit table. Returns true and fills the
    // canonical unit + multiplicative factor when the unit is known;
    // returns false (unknown) otherwise -- never guesses.
    static G4bool NormalizeUnit(const G4String& unit, G4String& canonicalUnit, double& factor);

    // Deterministic comparison block (multi-line, label : value) ending in a
    // `verdict :` line. `sim`/`ref` must already be normalized by the caller
    // or via Measurement(); this function normalizes internally as needed.
    static G4String Compare(const G4String& observable, const G4AgentMeasurement& sim,
                            const G4AgentMeasurement& ref, double thresholdSigma);

    static G4String VerdictName(G4AgentVerdict verdict);

  private:

    struct Extracted
    {
        G4String name;
        G4AgentMeasurement measurement;
        G4String source;  // provenance: file:line or "not available"
    };

    // Extract observables from runs/<id>/log.txt using validation/
    // observables.txt patterns. Empty vector + reason on failure.
    G4bool Extract(const G4String& runId, std::vector<Extracted>& out, G4String& reason) const;

    // Parse a references.txt-style file into (name -> measurement + source).
    G4bool LoadReferences(std::vector<Extracted>& out, G4String& reason) const;

    G4String Analyze(const G4String& runId) const;
    G4String CompareObservables(const G4String& runId) const;
    G4String Status() const;

    const G4AgentProject& fProject;
};

#endif