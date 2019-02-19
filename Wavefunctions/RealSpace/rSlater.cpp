/*
  Developed by Sandeep Sharma
  Copyright (c) 2017, Sandeep Sharma

  This file is part of DICE.

  This program is free software: you can redistribute it and/or modify it under the terms
  of the GNU General Public License as published by the Free Software Foundation,
  either version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with this program.
  If not, see <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#ifndef SERIAL
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include "igl/slice.h"
#include "igl/slice_into.h"

#include "Determinants.h"
#include "rDeterminants.h"
#include "rSlater.h"
#include "global.h"
#include "input.h"


using namespace Eigen;

rSlater::rSlater() 
{
  initHforbs();
  initDets();
}

int rSlater::getNumOfDets() const {return determinants.size();}


void rSlater::initHforbs() 
{
  int norbs = Determinant::norbs;
  int size; //dimension of the mo coeff matrix
  //initialize hftype and hforbs
  if (schd.hf == "rhf") {
    hftype = HartreeFock::Restricted;
    //hftype = 0;
    size = norbs;
  }
  else if (schd.hf == "uhf") {
    hftype = HartreeFock::UnRestricted;
    //hftype = 1;
    size = norbs;
  }
  else if (schd.hf == "ghf") {
    hftype = HartreeFock::Generalized;
    //hftype = 2;
    size = 2*norbs;
  }
  HforbsA = MatrixXd::Zero(size, size);
  HforbsB = MatrixXd::Zero(size, size);
  readHF(HforbsA, HforbsB, schd.hf);
}

void rSlater::initDets() 
{  
  int norbs = Determinant::norbs;
  int nalpha = rDeterminant::nalpha;
  int nbeta = rDeterminant::nbeta;
  
  //initialize slater determinants
  if (boost::iequals(schd.determinantFile, "")) {
    determinants.resize(1);
    ciExpansion.resize(1, 1.0);
    if (schd.hf == "rhf" || schd.hf == "uhf") {
      for (int i = 0; i < nalpha; i++)
        determinants[0].setoccA(i, true);
      for (int i = 0; i < nbeta; i++)
        determinants[0].setoccB(i, true);
    }
    else {
    //jailbreaking existing dets for ghf use, filling alpha orbitals first and then beta, a ghf det wrapper maybe cleaner
      int nelec = nalpha + nbeta;
      if (nelec <= norbs) {
        for (int i = 0; i < nelec; i++)
          determinants[0].setoccA(i, true);
      }
      else  {
        for (int i = 0; i < norbs; i++)
          determinants[0].setoccA(i, true);
        for (int i = 0; i < nelec-norbs; i++)
          determinants[0].setoccB(i, true);
      }
    }
  }
  else {
    readDeterminants(schd.determinantFile, determinants, ciExpansion);
  }
}

//I Assume that there is only single determinant in the ciexpansion
void rSlater::getVariables(Eigen::VectorBlock<VectorXd> &v) const
{ 
  int norbs = Determinant::norbs;  
  int nalpha = rDeterminant::nalpha;
  int nbeta = rDeterminant::nbeta;
  int nelec = nalpha + nbeta;
  
  for (int i = 0; i < determinants.size(); i++)
    v[i] = ciExpansion[i];
  int numDeterminants = determinants.size();

  if (hftype == Generalized) {
    for (int i = 0; i < 2*norbs; i++) {
      for (int j = 0; j < nelec; j++) 
        v[numDeterminants + i * nelec + j] = HforbsA(i, j);
    }
  }
  else {
    for (int i = 0; i < norbs; i++) {
      for (int j = 0; j < nalpha; j++) {
        v[numDeterminants + i * nelec + j] = HforbsA(i, j);
      }
    }
  
    if (hftype == UnRestricted) {
      for (int i = 0; i < norbs; i++) {
        for (int j = 0; j < nbeta; j++) {
          v[numDeterminants + nalpha * norbs + i * nbeta + j] = HforbsB(i, j);
        }
      }
    }
  }
}

long rSlater::getNumVariables() const
{
  int nalpha = rDeterminant::nalpha;
  int nbeta = rDeterminant::nbeta;
  int nelec = nalpha + nbeta;

  long numVars = 0;
  numVars += determinants.size();
  if (hftype == Restricted)
    numVars += nalpha * HforbsA.rows();
  else
    numVars += nelec * HforbsA.rows();
  return numVars;
}

void rSlater::updateVariables(const Eigen::VectorBlock<VectorXd> &v) 
{  
  int nalpha = rDeterminant::nalpha;
  int nbeta = rDeterminant::nbeta;
  int nelec = nalpha + nbeta;
  int norbs = Determinant::norbs;  

  for (int i = 0; i < determinants.size(); i++)
    ciExpansion[i] = v[i];
  int numDeterminants = determinants.size();

  if (hftype == Generalized) {
    for (int i = 0; i < 2*norbs; i++) {
      for (int j = 0; j < nelec; j++) {
        HforbsA(i, j) = v[numDeterminants + i * nelec + j];
        HforbsB(i, j) = v[numDeterminants + i * nelec + j];
      }
    } 
  }
  else {
    for (int i = 0; i < norbs; i++) {
      for (int j = 0; j < nalpha; j++) {
        HforbsA(i, j) = v[numDeterminants + i * nalpha + j];
        if (hftype == Restricted) {
          HforbsB(i, j) = v[numDeterminants + i * nalpha + j];
        }
      }
    }
    if (hftype == UnRestricted) {
      for (int i = 0; i < norbs; i++) {
        for (int j = 0; j < nbeta; j++) {
          HforbsB(i, j) = v[numDeterminants + nalpha * norbs + i * nbeta + j];
        }
      }
    }
  }
}

void rSlater::printVariables() const
{
  cout << endl<<"CI-expansion"<<endl;
  for (int i = 0; i < determinants.size(); i++) {
    cout << "  " << ciExpansion[i] << endl;
  }
  cout << endl<<"DeterminantA"<<endl;
  //for r/ghf
  for (int i = 0; i < HforbsA.rows(); i++) {
    for (int j = 0; j < HforbsA.rows(); j++)
      cout << "  " << HforbsA(i, j);
    cout << endl;
  }
  if (hftype == UnRestricted) {
  //if (hftype == 1) {
    cout << endl
         << "DeterminantB" << endl;
    for (int i = 0; i < HforbsB.rows(); i++) {
      for (int j = 0; j < HforbsB.rows(); j++)
        cout << "  " << HforbsB(i, j);
      cout << endl;
    }
  }
  cout << endl;

}
