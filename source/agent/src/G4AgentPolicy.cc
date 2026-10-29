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
#include "G4AgentPolicy.hh"

#include <algorithm>
#include <cctype>

namespace
{
G4String ToUpper(G4String s)
{
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
  return s;
}
}  // namespace

G4AgentPolicy G4AgentPolicy::FromString(const G4String& text)
{
  G4AgentPolicy policy;
  G4String u = ToUpper(text);
  if (u == "SUGGEST")
    policy.fLevel = Level::Suggest;
  else if (u == "EDIT")
    policy.fLevel = Level::Edit;
  else if (u == "AUTONOMOUS")
    policy.fLevel = Level::Autonomous;
  else
    policy.fLevel = Level::ReadOnly;  // empty/unrecognized fail closed
  return policy;
}

G4String G4AgentPolicy::Name() const
{
  switch (fLevel)
  {
    case Level::Suggest:
      return "SUGGEST";
    case Level::Edit:
      return "EDIT";
    case Level::Autonomous:
      return "AUTONOMOUS";
    default:
      return "READ_ONLY";
  }
}

G4String G4AgentPolicy::CheckTool(const G4String& toolName, G4bool toolIsReadOnly,
                                  G4bool requiresEdit) const
{
  // Unknown tools are always refused, at every level (fail closed).
  if (toolName.empty()) return "refused : empty tool name";

  if (requiresEdit)
  {
    if (fLevel == Level::Edit || fLevel == Level::Autonomous) return "";
    return "refused : tool '" + toolName + "' modifies project files; requires"
           " G4_AGENT_PERMISSION=EDIT (or AUTONOMOUS); current level " + Name();
  }

  if (toolIsReadOnly) return "";  // allowed at any level

  return "refused : tool '" + toolName
         + "' is not implemented in this agent build (future stage capability)";
}
