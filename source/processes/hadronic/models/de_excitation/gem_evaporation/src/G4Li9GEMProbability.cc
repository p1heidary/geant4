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
//
// $Id: G4Li9GEMProbability.cc,v 1.4 2005/06/04 13:25:25 jwellisc Exp $
// GEANT4 tag $Name: geant4-08-00 $
//
// Hadronic Process: Nuclear De-excitations
// by V. Lara (Nov 1999)
//


#include "G4Li9GEMProbability.hh"

G4Li9GEMProbability::G4Li9GEMProbability() :
  G4GEMProbability(9,3,3.0/2.0) // A,Z,Spin
{
  ExcitEnergies.push_back(4310.0*keV);
  ExcitSpins.push_back(0.0);
  ExcitLifetimes.push_back(hbar_Planck*std::log(2.0)/(100.0*keV));

  ExcitEnergies.push_back(5380.0*keV);
  ExcitSpins.push_back(0.0);
  ExcitLifetimes.push_back(hbar_Planck*std::log(2.0)/(600.0*keV));

  ExcitEnergies.push_back(6430.0*keV);
  ExcitSpins.push_back(9.0/2.0);
  ExcitLifetimes.push_back(hbar_Planck*std::log(2.0)/(40.0*keV));

  SetExcitationEnergiesPtr(&ExcitEnergies);
  SetExcitationSpinsPtr(&ExcitSpins);
  SetExcitationLifetimesPtr(&ExcitLifetimes);
}


G4Li9GEMProbability::G4Li9GEMProbability(const G4Li9GEMProbability &) : G4GEMProbability()
{
  throw G4HadronicException(__FILE__, __LINE__, "G4Li9GEMProbability::copy_constructor meant to not be accessable");
}




const G4Li9GEMProbability & G4Li9GEMProbability::
operator=(const G4Li9GEMProbability &)
{
  throw G4HadronicException(__FILE__, __LINE__, "G4Li9GEMProbability::operator= meant to not be accessable");
  return *this;
}


G4bool G4Li9GEMProbability::operator==(const G4Li9GEMProbability &) const
{
  return false;
}

G4bool G4Li9GEMProbability::operator!=(const G4Li9GEMProbability &) const
{
  return true;
}


