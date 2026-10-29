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
// G4AgentReport
//
// Class description
//
// The single deterministic text renderer for all [G4AGENT] output.
// A report is an optional header line followed by data lines and/or
// multi-line blocks; blocks come from the inspect and check services, 
// which compose them with the same stream formatting previously used 
// directly on G4cout, so the established label/value output contract 
// stays byte-identical.
//
// AsString() joins lines with newlines and never appends a trailing
// newline; callers stream the result with G4endl.

#ifndef G4AGENTREPORT_HH
#define G4AGENTREPORT_HH

#include "G4String.hh"

#include <vector>

class G4AgentReport
{
  public:

    G4AgentReport() = default;

    void SetHeader(const G4String& header);
    void AddLine(const G4String& line);

    // Append an already-composed block; the block is split into its
    // individual lines so reports never contain stray blank lines.
    void AddBlock(const G4String& block);

    G4String AsString() const;

  private:

    G4String fHeader;
    std::vector<G4String> fLines;
};

inline void G4AgentReport::SetHeader(const G4String& header)
{
  fHeader = header;
}

inline void G4AgentReport::AddLine(const G4String& line)
{
  fLines.push_back(line);
}

#endif
