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
// G4AgentPublication
//
// Class description
//
// Literature references and manuscript generation (Stage G), keeping the
// core strictly offline. Live literature retrieval (arXiv/web) is an
// optional external capability, not part of this class; here the operator
// supplies the sources and the agent preserves evidence and provenance.
//
//  1. Bibliography: a corpus of plain-text reference records under
//     <root>/references/ (G4_AGENT_REFERENCES_DIR overrides), one file per
//     source, `<id>.txt`, with `key : value` fields (title, authors, year,
//     venue, doi, arxiv, url, accessed, note). list/show/cite report only
//     the fields that are present -- no citation is ever invented.
//
//  2. Manuscript: deterministic Markdown assembled from persisted project
//     state (research notes, run manifests, the Stage-F comparison, the
//     reference corpus). Every section is tagged with its provenance class
//     --
//
//        [operator]     operator-authored narrative (objective, method)
//        [assumption]   operator-stated assumptions / estimates
//        [simulated]    values copied from run manifests
//        [derived]      values computed by the harness (comparisons)
//        [literature]   facts carrying a reference id
//        [experimental] reserved; only present if the operator supplies it
//
//     -- and missing inputs are printed as NOT AVAILABLE, never filled in.
//     `publish` writes paper/manuscript.md (EDIT-gated by the caller);
//     `manuscript_preview` returns the same text without writing.

#ifndef G4AGENTPUBLICATION_HH
#define G4AGENTPUBLICATION_HH

#include "G4AgentProject.hh"
#include "G4String.hh"

#include <vector>

class G4AgentAnalysis;

class G4AgentPublication
{
  public:

    G4AgentPublication(const G4AgentProject& project, const G4AgentAnalysis& analysis);

    // subcommand: status | list | show <id> | cite <id>
    G4String Bibliography(const G4String& subcommand, const G4String& argument) const;

    // Assemble the manuscript text (no writes).
    G4String Manuscript() const;

    // Render the manuscript and write <root>/paper/manuscript.md.
    G4String Publish() const;

    static G4String ConfiguredReferencesDir(const G4String& projectRoot);

  private:

    struct Reference
    {
        G4String id;
        G4String title;
        G4String authors;
        G4String year;
        G4String venue;
        G4String doi;
        G4String arxiv;
        G4String url;
        G4String accessed;
        G4String note;
    };

    G4bool LoadReferences(std::vector<Reference>& out, G4String& reason) const;
    static G4String Citation(const Reference& ref);

    // Optional project text file, confined; "" when absent.
    G4String ReadOptional(const G4String& relPath) const;

    const G4AgentProject& fProject;
    const G4AgentAnalysis& fAnalysis;
    G4AgentProject fReferences;  // corpus root (may be outside the project)
};

#endif