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
// $Id: IdentifiableQuantityVectorType.hh,v 1.3 2002/07/25 10:41:20 radoone Exp $
// GEANT4 tag $Name: geant4-05-02 $
//
// 
// --------------------------------------------------------------
// Comments
//
// --------------------------------------------------------------
//
#ifndef IDENTIFIABLEQUANTITYVECTORTYPE_H
#define IDENTIFIABLEQUANTITYVECTORTYPE_H 1


#include "QuantityVectorType.hh"


class IdentifiableQuantityVectorType : public QuantityVectorType {
public:
  IdentifiableQuantityVectorType() {
  }
  ~IdentifiableQuantityVectorType() {
  }
  void set_name( const std::string& s ) {
    m_name = s;
  }
  void set_name( const char* s ) {
    m_name = s;
  }
  std::string get_name() const {
    return m_name;
  }
private:
  // ID required
  std::string m_name;    
};


#endif // IDENTIFIABLEQUANTITYVECTORTYPE_H