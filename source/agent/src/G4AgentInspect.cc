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

#include "G4AgentInspect.hh"

#include "G4AgentLLMConfig.hh"
#include "G4AgentManager.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Material.hh"
#include "G4ParticleDefinition.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessTable.hh"
#include "G4ProcessType.hh"
#include "G4ProcessVector.hh"
#include "G4ProductionCuts.hh"
#include "G4ProductionCutsTable.hh"
#include "G4RegionStore.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4RunManagerKernel.hh"
#include "G4StateManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4VModularPhysicsList.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VPhysicsConstructor.hh"
#include "G4VProcess.hh"
#include "G4VSolid.hh"
#include "G4VUserPhysicsList.hh"
#include "G4VVisManager.hh"
#include "G4Version.hh"
#include "G4ios.hh"

#include <cxxabi.h>

#include <cstdlib>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>

namespace
{

// Upper bound for iterating the modular physics constructor vector, which
// has no public size accessor; GetPhysics(i) returns nullptr past the end.
constexpr G4int kMaxPhysicsConstructors = 256;

const char* RunManagerTypeName(G4RunManager::RMType type)
{
  switch (type)
  {
    case G4RunManager::sequentialRM:
      return "sequential";
    case G4RunManager::masterRM:
      return "master";
    case G4RunManager::workerRM:
      return "worker";
    case G4RunManager::subEventMasterRM:
      return "subEventMaster";
    case G4RunManager::subEventWorkerRM:
      return "subEventWorker";
    default:
      return "unknown";
  }
}

std::string Demangle(const char* name)
{
#ifdef __GNUG__
  int status = 0;
  std::unique_ptr<char, decltype(&std::free)> res(
    abi::__cxa_demangle(name, nullptr, nullptr, &status), &std::free);
  return res ? std::string(res.get()) : std::string(name);
#else
  return name;
#endif
}

// Remove one trailing newline so blocks compose without blank lines.
std::string TrimNewline(std::string text)
{
  if (!text.empty() && text.back() == '\n') text.pop_back();
  return text;
}

}  // namespace

G4String G4AgentInspect::StatusFields() const
{
  G4RunManager* runManager = G4RunManager::GetRunManager();

  G4String stateString = "NOT AVAILABLE";
  if (G4StateManager::GetStateManager() != nullptr)
  {
    G4ApplicationState state = G4StateManager::GetStateManager()->GetCurrentState();
    stateString = G4StateManager::GetStateManager()->GetStateString(state);
  }

  std::ostringstream os;
  os << "Geant4 version             : " << G4AgentManager::GetGeant4Version() << "\n";
  os << "Agent interface version    : " << G4AgentManager::GetInterfaceVersion() << "\n";
  os << "Application state          : " << stateString << "\n";
  os << "Run manager type           : "
     << (runManager != nullptr ? RunManagerTypeName(runManager->GetRunManagerType())
                               : "NOT AVAILABLE")
     << "\n";
  os << "Multithreading             : "
     << (G4Threading::IsMultithreadedApplication() ? "active" : "sequential") << "\n";
  os << "Thread                     : " << (G4Threading::IsWorkerThread() ? "worker" : "master")
     << "\n";
  os << "UI available               : " << (G4UImanager::GetUIpointer() != nullptr ? "yes" : "no")
     << "\n";
  os << "Visualization              : "
     << (G4VVisManager::GetConcreteInstance() != nullptr ? "available" : "not available") << "\n";
  os << "LLM                        : " << G4AgentLLMConfig::FromEnvironment().Describe();

  return os.str();
}

G4String G4AgentInspect::VersionBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] version\n";
  os << "Geant4 version          : " << G4AgentManager::GetGeant4Version() << "\n";
  os << "Geant4 date             : " << G4Date << "\n";
  os << "Agent interface version : " << G4AgentManager::GetInterfaceVersion();
  return os.str();
}

G4String G4AgentInspect::CapabilitiesBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] capabilities\n";
  os << "agent-interface : " << G4AgentManager::GetInterfaceVersion() << "\n";
  os << "status\n";
  os << "version\n";
  os << "ui-list\n";
  os << "particle-list\n";
  os << "process-list\n";
  os << "material-list\n";
  os << "volume-list\n";
  os << "application-inspection\n";
  os << "geometry-inspection\n";
  os << "physics-inspection\n";
  os << "run-inspection\n";
  os << "history-inspection\n";
  os << "tracking-inspection\n";
  os << "geometry-check\n";
  os << "physics-check\n";
  os << "run-check\n";
  os << "report\n";
  os << "llm-status\n";
  os << "ask\n";
  os << "tool-loop\n";
  os << "project-tools : list/read/search/diff (read-only, confined)\n";
  os << "controlled-dev : run_macro/project_write/project_build"
        " (G4_AGENT_PERMISSION-gated)\n";
  os << "knowledge-local : optional local corpus"
        " (G4_AGENT_KNOWLEDGE_DIR); no network\n";
  os << "permission-modes : READ_ONLY (default) | SUGGEST | EDIT | AUTONOMOUS\n";
  os << "                   (only read-only tools exist today)\n";
  os << "output     : text\n";
  os << "ai         : optional bounded tool loop (actuating tools gated)\n";
  os << "llm        : " << G4AgentLLMConfig::FromEnvironment().Describe() << "\n";
  os << "network    : core no; optional G4agent_llm module only";
  return os.str();
}

void G4AgentInspect::PrintUIList(const G4String& directory) const
{
  G4UImanager* uiManager = G4UImanager::GetUIpointer();
  G4cout << "[G4AGENT] list ui" << G4endl;
  if (uiManager == nullptr)
  {
    G4cout << "UI manager : NOT AVAILABLE" << G4endl;
    return;
  }
  // Reuse the existing UI manager facility directly; capturing and
  // reformatting its output would duplicate a public Geant4 service.
  uiManager->ListCommands(directory.c_str());
}

G4String G4AgentInspect::ParticleListBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] list particles\n";
  G4ParticleTable* table = G4ParticleTable::GetParticleTable();
  if (table == nullptr)
  {
    os << "count : NOT AVAILABLE";
    return os.str();
  }
  const G4int count = table->entries();
  os << "count : " << count << "\n";
  for (G4int i = 0; i < count; ++i)
  {
    G4ParticleDefinition* particle = table->GetParticle(i);
    if (particle != nullptr)
      os << "  " << particle->GetParticleName() << "  pdg " << particle->GetPDGEncoding() << "\n";
  }
  return TrimNewline(os.str());
}

G4String G4AgentInspect::ProcessListBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] list processes\n";
  G4ProcessTable* table = G4ProcessTable::GetProcessTable();
  if (table == nullptr)
  {
    os << "count : NOT AVAILABLE";
    return os.str();
  }
  G4ProcessTable::G4ProcNameVector* names = table->GetNameList();
  if (names == nullptr)
  {
    os << "count : 0";
    return os.str();
  }
  os << "count : " << names->size() << "\n";
  for (const auto& name : *names)
    os << "  " << name << "\n";
  return TrimNewline(os.str());
}

G4String G4AgentInspect::MaterialListBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] list materials\n";
  const G4MaterialTable* materials = G4Material::GetMaterialTable();
  const std::size_t count = G4Material::GetNumberOfMaterials();
  os << "count : " << count << "\n";
  for (std::size_t i = 0; i < count; ++i)
  {
    const G4Material* material = (*materials)[i];
    if (material != nullptr)
      os << "  " << material->GetName() << "  rho " << material->GetDensity() / g / cm3
         << " g/cm3\n";
  }
  return TrimNewline(os.str());
}

G4String G4AgentInspect::VolumeListBlock() const
{
  std::ostringstream os;
  os << "[G4AGENT] list volumes\n";
  G4PhysicalVolumeStore* store = G4PhysicalVolumeStore::GetInstance();
  if (store == nullptr)
  {
    os << "count : NOT AVAILABLE";
    return os.str();
  }
  os << "count : " << store->size() << "\n";
  for (const auto& volume : *store)
  {
    if (volume != nullptr)
    {
      G4String type = "unknown";
      if (volume->GetLogicalVolume() != nullptr
          && volume->GetLogicalVolume()->GetSolid() != nullptr)
        type = volume->GetLogicalVolume()->GetSolid()->GetEntityType();
      os << "  " << volume->GetName() << ":" << volume->GetCopyNo() << "  " << type << "\n";
    }
  }
  return TrimNewline(os.str());
}

G4String G4AgentInspect::ApplicationFields() const
{
  G4StateManager* stateManager = G4StateManager::GetStateManager();
  G4RunManager* runManager = G4RunManager::GetRunManager();

  std::ostringstream os;
  if (stateManager != nullptr)
    os << "application state : " << stateManager->GetStateString(stateManager->GetCurrentState())
       << "\n";

  if (runManager != nullptr)
  {
    os << "run manager type  : " << RunManagerTypeName(runManager->GetRunManagerType()) << "\n";
    os << "initialized       : "
       << ((stateManager != nullptr && stateManager->GetCurrentState() != G4State_PreInit
            && stateManager->GetCurrentState() != G4State_Init)
             ? "yes"
             : "no")
       << "\n";
  }
  else
  {
    os << "run manager type  : NOT AVAILABLE\n";
  }

  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  os << "physics list      : "
     << ((kernel != nullptr && kernel->GetPhysicsList() != nullptr) ? "present" : "absent") << "\n";
  os << "world volume      : "
     << ((kernel != nullptr && kernel->GetCurrentWorld() != nullptr) ? "present" : "absent")
     << "\n";
  if (runManager != nullptr)
  {
    os << "detector          : "
       << (runManager->GetUserDetectorConstruction() != nullptr ? "present" : "absent") << "\n";
    os << "user actions      : "
       << (runManager->GetUserActionInitialization() != nullptr ? "present" : "absent") << "\n";
    os << "primary generator : "
       << (runManager->GetUserPrimaryGeneratorAction() != nullptr ? "present" : "absent") << "\n";
  }
  os << "ui active         : " << (G4UImanager::GetUIpointer() != nullptr ? "yes" : "no");
  return os.str();
}

G4String G4AgentInspect::GeometryBlock(const G4String& scope) const
{
  if (scope.empty() || scope == "summary") return GeometryFields();

  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  std::ostringstream os;

  if (scope == "world")
  {
    G4VPhysicalVolume* world = (kernel != nullptr) ? kernel->GetCurrentWorld() : nullptr;
    if (world == nullptr)
    {
      os << "world : NOT AVAILABLE (geometry not initialized)";
      return os.str();
    }
    os << "world             : " << world->GetName() << ":" << world->GetCopyNo() << "\n";
    os << "root logical      : "
       << (world->GetLogicalVolume() != nullptr ? world->GetLogicalVolume()->GetName() : "unknown")
       << "\n";
    if (world->GetLogicalVolume() != nullptr)
      os << "direct daughters  : " << world->GetLogicalVolume()->GetNoDaughters();
    return os.str();
  }

  G4PhysicalVolumeStore* physicalStore = G4PhysicalVolumeStore::GetInstance();
  G4VPhysicalVolume* volume = nullptr;
  if (physicalStore != nullptr) volume = physicalStore->GetVolume(scope, false);
  if (volume != nullptr)
  {
    G4LogicalVolume* logical = volume->GetLogicalVolume();
    os << "volume            : " << volume->GetName() << ":" << volume->GetCopyNo() << "\n";
    os << "logical           : " << (logical != nullptr ? logical->GetName() : "unknown") << "\n";
    if (logical != nullptr)
    {
      os << "solid             : "
         << (logical->GetSolid() != nullptr ? logical->GetSolid()->GetEntityType() : "unknown")
         << "\n";
      os << "material          : "
         << (logical->GetMaterial() != nullptr ? logical->GetMaterial()->GetName() : "unknown")
         << "\n";
      os << "daughters         : " << logical->GetNoDaughters();
    }
  }
  else
  {
    os << "volume            : '" << scope << "' not found";
  }
  return os.str();
}

G4String G4AgentInspect::GeometryFields() const
{
  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  G4VPhysicalVolume* world = (kernel != nullptr) ? kernel->GetCurrentWorld() : nullptr;

  G4LogicalVolumeStore* logicalStore = G4LogicalVolumeStore::GetInstance();
  G4PhysicalVolumeStore* physicalStore = G4PhysicalVolumeStore::GetInstance();
  const std::size_t nLogical = (logicalStore != nullptr) ? logicalStore->size() : 0;
  const std::size_t nPhysical = (physicalStore != nullptr) ? physicalStore->size() : 0;

  std::ostringstream os;
  os << "world             : " << (world != nullptr ? world->GetName() : "NOT AVAILABLE") << "\n";
  os << "logical volumes   : " << nLogical << "\n";
  os << "physical volumes  : " << nPhysical << "\n";
  os << "materials         : " << G4Material::GetNumberOfMaterials() << "\n";
  if (G4RegionStore::GetInstance() != nullptr)
    os << "regions           : " << G4RegionStore::GetInstance()->size();
  else
    os << "regions           : NOT AVAILABLE";
  return os.str();
}

G4String G4AgentInspect::PhysicsFields() const
{
  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  G4VUserPhysicsList* physics = (kernel != nullptr) ? kernel->GetPhysicsList() : nullptr;
  std::ostringstream os;
  if (physics == nullptr)
  {
    os << "physics list : NOT AVAILABLE";
    return os.str();
  }
  os << "physics list : " << Demangle(typeid(*physics).name()) << "\n";

  auto* modular = dynamic_cast<G4VModularPhysicsList*>(physics);
  if (modular != nullptr)
  {
    os << "modular      : yes\n";
    for (G4int i = 0; i < kMaxPhysicsConstructors; ++i)
    {
      const G4VPhysicsConstructor* constructor = modular->GetPhysics(i);
      if (constructor == nullptr) break;
      os << "  constructor : " << constructor->GetPhysicsName() << "\n";
    }
  }
  else
  {
    os << "modular      : no\n";
  }

  G4ParticleTable* table = G4ParticleTable::GetParticleTable();
  const G4int count = (table != nullptr) ? table->entries() : 0;
  os << "particles    : " << count << "\n";

  const auto* cutsTable = G4ProductionCutsTable::GetProductionCutsTable();
  const auto* defaultCuts =
    (cutsTable != nullptr) ? cutsTable->GetDefaultProductionCuts() : nullptr;
  if (defaultCuts == nullptr)
  {
    os << "prod cuts    : NOT AVAILABLE (physics not initialized)\n";
  }
  else
  {
    os << "prod cuts    : default (gamma/e-/e+/p) in mm :"
       << " " << defaultCuts->GetProductionCut(idxG4GammaCut) / mm << " "
       << defaultCuts->GetProductionCut(idxG4ElectronCut) / mm << " "
       << defaultCuts->GetProductionCut(idxG4PositronCut) / mm << " "
       << defaultCuts->GetProductionCut(idxG4ProtonCut) / mm << "\n";
    if (cutsTable->GetTableSize() > 0)
      os << "  couple table entries : " << cutsTable->GetTableSize() << "\n";
  }

  std::set<int> processTypes;
  std::set<std::string> processNames;
  for (G4int i = 0; i < count; ++i)
  {
    G4ParticleDefinition* particle = table->GetParticle(i);
    if (particle == nullptr) continue;
    G4ProcessManager* manager = particle->GetProcessManager();
    if (manager == nullptr) continue;
    G4ProcessVector* list = manager->GetProcessList();
    if (list == nullptr) continue;
    for (std::size_t j = 0; j < list->size(); ++j)
    {
      G4VProcess* process = (*list)[j];
      if (process == nullptr) continue;
      processTypes.insert(process->GetProcessType());
      processNames.insert(process->GetProcessName());
    }
  }
  os << "process categories : ";
  for (const int type : processTypes)
    os << G4VProcess::GetProcessTypeName(static_cast<G4ProcessType>(type)) << " ";
  os << "\n";
  os << "distinct processes : " << processNames.size() << "\n";

  std::set<int> categories = {fTransportation, fElectromagnetic, fOptical,
                              fHadronic,       fDecay,           fGeneral};
  for (const int type : categories)
  {
    G4String label = G4VProcess::GetProcessTypeName(static_cast<G4ProcessType>(type));
    os << "  " << label << " : "
       << (processTypes.find(type) != processTypes.end() ? "present" : "absent") << "\n";
  }
  os << "note : process presence is a structural observation only; it does not "
        "certify that the corresponding physics capability is configured "
        "correctly.";
  return os.str();
}

G4String G4AgentInspect::RunFields() const
{
  G4RunManager* runManager = G4RunManager::GetRunManager();
  std::ostringstream os;
  if (runManager == nullptr)
  {
    os << "run manager : NOT AVAILABLE";
    return os.str();
  }
  if (G4StateManager::GetStateManager() != nullptr)
    os << "state              : "
       << G4StateManager::GetStateManager()->GetStateString(
            G4StateManager::GetStateManager()->GetCurrentState())
       << "\n";
  os << "run manager type   : " << RunManagerTypeName(runManager->GetRunManagerType()) << "\n";
  os << "events to process  : " << runManager->GetNumberOfEventsToBeProcessed() << "\n";
  os << "threads            : " << runManager->GetNumberOfThreads() << "\n";
  const G4Run* currentRun = runManager->GetCurrentRun();
  if (currentRun != nullptr)
  {
    os << "current run id     : " << currentRun->GetRunID() << "\n";
    os << "events in run      : " << currentRun->GetNumberOfEvent() << "\n";
    os << "run events requested : " << currentRun->GetNumberOfEventToBeProcessed();
  }
  else
  {
    os << "current run id     : none";
  }
  return os.str();
}

G4String G4AgentInspect::HistoryBlock() const
{
  std::ostringstream os;
  G4UImanager* uiManager = G4UImanager::GetUIpointer();
  if (uiManager == nullptr)
  {
    os << "count : NOT AVAILABLE";
    return os.str();
  }
  const G4int count = uiManager->GetNumberOfHistory();
  os << "count : " << count << "\n";
  for (G4int i = 0; i < count; ++i)
    os << "  " << uiManager->GetPreviousCommand(i) << "\n";
  return TrimNewline(os.str());
}

G4String G4AgentInspect::TrackingBlock() const
{
  std::ostringstream os;
  os << "thread : " << (G4Threading::IsWorkerThread() ? "worker" : "master") << "\n";
  G4RunManagerKernel* kernel = G4RunManagerKernel::GetRunManagerKernel();
  os << "tracking manager : "
     << ((kernel != nullptr && kernel->GetTrackingManager() != nullptr) ? "available"
                                                                        : "NOT AVAILABLE")
     << "\n";
  os << "note : no track-level agent state is exposed in this interface version.";
  return os.str();
}
