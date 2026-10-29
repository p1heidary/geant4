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
// G4AgentKnowledge
//
// Class description
//
// Local, optional knowledge retrieval. The corpus
// is a plain-text directory (G4_AGENT_KNOWLEDGE_DIR, default: knowledge/
// under the project root) provided by the user or the operator -- never
// bundled into the Geant4 binary, never fetched from the network. Absence
// of a corpus is a normal, reported state.
//
// Confinement, reads and search are REUSED from G4AgentProject rooted at
// the corpus directory (no second file-safety implementation).
// Documents may carry a simple metadata header (Source: / Version: /
// Category:) that the catalog reports next to the toolkit version, keeping
// source authority and version context visible.
//
// knowledge_record_gap appends to GAPS.md inside the corpus: the agent's
// own record of what it could not answer, per "record actual knowledge
// gaps" . It is append-only, confined and plain text.

#ifndef G4AGENTKNOWLEDGE_HH
#define G4AGENTKNOWLEDGE_HH

#include "G4AgentProject.hh"
#include "G4String.hh"

class G4AgentKnowledge
{
  public:

    G4AgentKnowledge(const G4String& corpusDir, const G4String& toolkitVersion);

    static G4String ConfiguredCorpusDir(const G4String& projectRoot);

    // subcommand: status | catalog | search | gap
    G4String Run(const G4String& subcommand, const G4String& argument) const;

    G4bool Available() const;

  private:

    G4String Catalog() const;
    G4String Status() const;
    G4String RecordGap(const G4String& note) const;

    G4AgentProject fCorpus;
    G4String fToolkitVersion;
    G4bool fConfigured = false;
};

#endif
