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
// G4AgentPolicy
//
// Class description
//
// Permission gate for agent tool execution. The reasoning loop in
// G4Agent::Ask consults the policy before executing any
// tool the model requests. The default level is READ_ONLY, and all semantic
// tools defined today are read-only inspection/checking, so they are
// allowed. Unknown or non-read-only tool names are refused with a
// deterministic reason. The higher levels (SUGGEST, EDIT, AUTONOMOUS) parse
// and report; no write, edit or shell capability exists in this subsystem,
// so setting G4_AGENT_PERMISSION grants nothing — capabilities arrive with
// their stages and check their level first (fail closed).

#ifndef G4AGENTPOLICY_HH
#define G4AGENTPOLICY_HH

#include "G4String.hh"

class G4AgentPolicy
{
  public:

    enum class Level
    {
      ReadOnly,
      Suggest,
      Edit,
      Autonomous
    };

    // Parse "READ_ONLY|SUGGEST|EDIT|AUTONOMOUS" (case-insensitive);
    // empty/unrecognized -> ReadOnly (fail closed).
    static G4AgentPolicy FromString(const G4String& text);

    Level GetLevel() const { return fLevel; }
    G4String Name() const;

    // Tool permission decision. toolIsReadOnly marks observation/checking
    // tools (allowed at any level). Modifying tools need explicit
    // permission: project_write requires EDIT or AUTONOMOUS (a write is an
    // edit, not a suggestion); unknown or unclassified names are refused at
    // every level (fail closed).
    G4String CheckTool(const G4String& toolName, G4bool toolIsReadOnly,
                       G4bool requiresEdit = false) const;

  private:

    Level fLevel = Level::ReadOnly;
};

#endif
