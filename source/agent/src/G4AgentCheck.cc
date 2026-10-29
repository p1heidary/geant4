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

#include "G4AgentCheck.hh"

#include "G4LogicalVolume.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessType.hh"
#include "G4ProcessVector.hh"
#include "G4RunManager.hh"
#include "G4RunManagerKernel.hh"
#include "G4StateManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4VUserPhysicsList.hh"

#include <sstream>

namespace
{
std::string TrimNewline(std::string text)
{
  if (!text.empty() && text.back() == '\n') text.pop_back();
  return text;
}
}  // namespace

G4String G4AgentCheck::GeometryCheck(const G4String& scope) const
{
  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  G4PhysicalVolumeStore* store = G4PhysicalVolumeStore::GetInstance();
  std::ostringstream os;

  // Geant4's CheckOverlaps tests a placed volume against its mother and its
  // sisters; it is a no-op on the top-level world (which has no mother).
  // Therefore "world" scope must be checked by visiting every placement,
  // and a named volume is checked directly.
  if (scope.empty() || scope == "world")
  {
    G4VPhysicalVolume* world = (kernel != nullptr) ? kernel->GetCurrentWorld() : nullptr;
    if (world == nullptr || store == nullptr)
    {
      os << "result : NOT AVAILABLE (geometry not initialized)";
      return os.str();
    }
    std::size_t checked = 0;
    G4bool overlap = false;
    G4String offender;
    for (const auto& pv : *store)
    {
      if (pv == nullptr || pv->GetMotherLogical() == nullptr) continue;
      ++checked;
      if (pv->CheckOverlaps(1000, 0., false, 1))
      {
        overlap = true;
        if (offender.empty()) offender = pv->GetName() + ":" + std::to_string(pv->GetCopyNo());
      }
    }
    os << "scope          : world\n";
    os << "volumes checked: " << checked << "\n";
    os << "result         : " << (overlap ? "FAIL" : "PASS") << "\n";
    if (overlap) os << "overlap in     : " << offender << "\n";
    os << "note           : overlap-only test via the Geant4 overlap engine; it "
          "does not certify the full geometry.";
    return os.str();
  }

  G4VPhysicalVolume* volume = (store != nullptr) ? store->GetVolume(scope, false) : nullptr;
  if (volume == nullptr)
  {
    os << "result : volume '" << scope << "' not found";
    return os.str();
  }
  if (volume->GetMotherLogical() == nullptr)
  {
    os << "checked volume : " << volume->GetName() << "\n";
    os << "result         : NOT APPLICABLE (top-level volume has no mother)\n";
    os << "note           : use scope 'world' to check every placement.";
    return os.str();
  }
  const G4bool overlap = volume->CheckOverlaps(1000, 0., false, 1);
  os << "checked volume : " << volume->GetName() << "\n";
  os << "result         : " << (overlap ? "FAIL" : "PASS") << "\n";
  os << "note           : overlap-only test via the Geant4 overlap engine; it "
        "does not certify the full geometry.";
  return os.str();
}

G4String G4AgentCheck::PhysicsCheck() const
{
  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  G4VUserPhysicsList* physics = (kernel != nullptr) ? kernel->GetPhysicsList() : nullptr;

  std::ostringstream os;
  os << "physics list : " << (physics != nullptr ? "PASS" : "FAIL") << "\n";

  G4StateManager* stateManager = G4StateManager::GetStateManager();
  os << "initialized  : "
     << ((stateManager != nullptr && stateManager->GetCurrentState() != G4State_PreInit
          && stateManager->GetCurrentState() != G4State_Init)
           ? "PASS"
           : "WARNING")
     << "\n";

  G4ParticleTable* table = G4ParticleTable::GetParticleTable();
  os << "particles    : " << ((table != nullptr && table->entries() > 0) ? "PASS" : "FAIL") << "\n";

  bool transported = false;
  if (table != nullptr)
  {
    G4ParticleDefinition* photon = table->FindParticle("gamma");
    if (photon != nullptr && photon->GetProcessManager() != nullptr)
    {
      G4ProcessVector* list = photon->GetProcessManager()->GetProcessList();
      if (list != nullptr)
      {
        for (std::size_t i = 0; i < list->size(); ++i)
        {
          if ((*list)[i] != nullptr && (*list)[i]->GetProcessType() == fTransportation)
            transported = true;
        }
      }
    }
  }
  os << "transportation : " << (transported ? "PASS" : "NOT AVAILABLE") << "\n";
  os << "note : structural check only; this is not scientific validation.";
  return TrimNewline(os.str());
}

G4String G4AgentCheck::RunCheck() const
{
  G4RunManager* runManager = G4RunManager::GetRunManager();
  std::ostringstream os;
  os << "run manager : " << (runManager != nullptr ? "PASS" : "FAIL") << "\n";
  G4StateManager* stateManager = G4StateManager::GetStateManager();
  if (stateManager != nullptr)
  {
    const bool ready = stateManager->GetCurrentState() != G4State_PreInit
                       && stateManager->GetCurrentState() != G4State_Init;
    os << "initialized : " << (ready ? "PASS" : "WARNING") << "\n";
  }
  else
  {
    os << "initialized : NOT AVAILABLE\n";
  }
  if (runManager != nullptr) os << "threads     : " << runManager->GetNumberOfThreads();
  return TrimNewline(os.str());
}
