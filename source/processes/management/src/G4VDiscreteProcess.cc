// This code implementation is the intellectual property of
// the RD44 GEANT4 collaboration.
//
// By copying, distributing or modifying the Program (or any work
// based on the Program) you indicate your acceptance of this statement,
// and all its terms.
//
// $Id: G4VDiscreteProcess.cc,v 2.0 1998/07/02 16:39:53 gunter Exp $
// GEANT4 tag $Name: geant4-00 $
//
// 
// --------------------------------------------------------------
//	GEANT 4 class implementation file 
//
//	For information related to this code contact:
//	CERN, CN Division, ASD Group
//	History: first implementation, based on object model of
//	2nd December 1995, G.Cosmo
// --------------------------------------------------------------
//   New Physics scheme           8 Jan. 1997  H.Kurahige
// ------------------------------------------------------------

#include "G4VDiscreteProcess.hh"
G4VDiscreteProcess::G4VDiscreteProcess()
                   :G4VProcess("No Name Discrete Process") 
{
  G4Exception("G4VDiscreteProcess:: default constructor is called");
}

G4VDiscreteProcess::G4VDiscreteProcess(const G4String& aName , G4ProcessType aType)
                  : G4VProcess(aName, aType)
{
}

G4VDiscreteProcess::~G4VDiscreteProcess()
{
}

G4VDiscreteProcess::G4VDiscreteProcess(G4VDiscreteProcess& right)
                  : G4VProcess(right)
{
}










