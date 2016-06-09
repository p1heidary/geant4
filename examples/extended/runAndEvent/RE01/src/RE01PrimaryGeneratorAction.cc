//
// ********************************************************************
// * DISCLAIMER                                                       *
// *                                                                  *
// * The following disclaimer summarizes all the specific disclaimers *
// * of contributors to this software. The specific disclaimers,which *
// * govern, are listed with their locations in:                      *
// *   http://cern.ch/geant4/license                                  *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.                                                             *
// *                                                                  *
// * This  code  implementation is the  intellectual property  of the *
// * GEANT4 collaboration.                                            *
// * By copying,  distributing  or modifying the Program (or any work *
// * based  on  the Program)  you indicate  your  acceptance of  this *
// * statement, and all its terms.                                    *
// ********************************************************************
//
// $Id: RE01PrimaryGeneratorAction.cc,v 1.1 2004/11/26 07:37:42 asaim Exp $
// GEANT4 tag $Name: geant4-08-00 $
//


#include "RE01PrimaryGeneratorAction.hh"

#include "G4Event.hh"
#include "G4HEPEvtInterface.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "RE01PrimaryGeneratorMessenger.hh"

RE01PrimaryGeneratorAction::RE01PrimaryGeneratorAction()
{
  const char* filename = "pythia_event.data";
  HEPEvt = new G4HEPEvtInterface(filename);

  G4int n_particle = 1;
  G4ParticleGun* fParticleGun = new G4ParticleGun(n_particle);
  G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
  G4String particleName;
  G4ParticleDefinition* particle
    = particleTable->FindParticle(particleName="mu+");
  fParticleGun->SetParticleDefinition(particle);
  fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0.,1.,0.));
  fParticleGun->SetParticleEnergy(100.*GeV);
  fParticleGun->SetParticlePosition(G4ThreeVector(0.*cm,0.*cm,0.*cm));
  particleGun = fParticleGun;

  messenger = new RE01PrimaryGeneratorMessenger(this);
  useHEPEvt = true;
}

RE01PrimaryGeneratorAction::~RE01PrimaryGeneratorAction()
{
  delete HEPEvt;
  delete particleGun;
  delete messenger;
}

void RE01PrimaryGeneratorAction::GeneratePrimaries(G4Event* anEvent)
{
  if(useHEPEvt)
  { HEPEvt->GeneratePrimaryVertex(anEvent); }
  else
  { particleGun->GeneratePrimaryVertex(anEvent); }
}

