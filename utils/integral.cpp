/*
  Developed by Sandeep Sharma with contributions from James E. T. Smith and Adam A. Holmes, 2017
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
#include "integral.h"
#include "string.h"
#include "global.h"
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include "math.h"
#include "boost/format.hpp"
#include <fstream>
#include "Determinants.h"
#ifndef SERIAL
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif

using namespace boost;

bool myfn(double i, double j) { return fabs(i)<fabs(j); }



//=============================================================================
void readIntegralsAndInitializeDeterminantStaticVariables(string fcidump) {
//-----------------------------------------------------------------------------
    /*!
    Read FCIDUMP file and populate "I1, I2, coreE, nelec, norbs, irrep"

    :Inputs:

        string fcidump:
            Name of the FCIDUMP file
    */
//-----------------------------------------------------------------------------
#ifndef SERIAL
  boost::mpi::communicator world;
#endif
  ifstream dump(fcidump.c_str());
  if (!dump.good()) {
    cout << "Integral file "<<fcidump<<" does not exist!"<<endl;
    exit(0);
  }
  vector<int> irrep;
  int nelec, sz, norbs, nalpha, nbeta;

#ifndef SERIAL
  if (commrank == 0) {
#endif
    I2.ksym = false;
    bool startScaling = false;
    norbs = -1;
    nelec = -1;
    sz = -1;

    int index = 0;
    vector<string> tok;
    string msg;
    while(!dump.eof()) {
      std::getline(dump, msg);
      trim(msg);
      boost::split(tok, msg, is_any_of(", \t="), token_compress_on);

      if (startScaling == false && tok.size() == 1 && (boost::iequals(tok[0],"&END") || boost::iequals(tok[0], "/"))) {
        startScaling = true;
        index += 1;
        break;
      } else if(startScaling == false) {
        if (boost::iequals(tok[0].substr(0,4),"&FCI")) {
          if (boost::iequals(tok[1].substr(0,4), "NORB"))
            norbs = atoi(tok[2].c_str());
          if (boost::iequals(tok[3].substr(0,5), "NELEC"))
            nelec = atoi(tok[4].c_str());
          if (boost::iequals(tok[5].substr(0,5), "MS2"))
            sz = atoi(tok[6].c_str());
        } else if (boost::iequals(tok[0].substr(0,4),"ISYM")) {
          continue;
        } else if (boost::iequals(tok[0].substr(0,4),"KSYM")) {
          I2.ksym = true;
        } else if (boost::iequals(tok[0].substr(0,6),"ORBSYM")) {
          for (int i=1;i<tok.size(); i++)
            irrep.push_back(atoi(tok[i].c_str()));
        } else {
          for (int i=0;i<tok.size(); i++)
            irrep.push_back(atoi(tok[i].c_str()));
        }
        index += 1;
      }
    } // while

    if (norbs == -1 || nelec == -1 || sz == -1) {
      std::cout << "could not read the norbs or nelec or MS2"<<std::endl;
      exit(0);
    }
    nalpha = nelec/2 + sz;
    nbeta = nelec - nalpha;
    irrep.resize(norbs);

#ifndef SERIAL
  } // commrank=0

  mpi::broadcast(world, nalpha, 0);
  mpi::broadcast(world, nbeta, 0);
  mpi::broadcast(world, nelec, 0);
  mpi::broadcast(world, norbs, 0);
  mpi::broadcast(world, irrep, 0);
  mpi::broadcast(world, I2.ksym, 0);
#endif

  long npair = norbs*(norbs+1)/2;
  if (I2.ksym) npair = norbs*norbs;
  I2.norbs = norbs;
  size_t I2memory = npair*(npair+1)/2; //memory in bytes

#ifndef SERIAL
  world.barrier();
#endif

  int2Segment.truncate((I2memory)*sizeof(double));
  regionInt2 = boost::interprocess::mapped_region{int2Segment, boost::interprocess::read_write};
  memset(regionInt2.get_address(), 0., (I2memory)*sizeof(double));

#ifndef SERIAL
  world.barrier();
#endif

  I2.store = static_cast<double*>(regionInt2.get_address());

  if (commrank == 0) {

    I1.store.clear();
    I1.store.resize(2*norbs*(2*norbs),0.0); I1.norbs = 2*norbs;
    coreE = 0.0;

    vector<string> tok;
    string msg;
    while(!dump.eof()) {
      std::getline(dump, msg);
      trim(msg);
      boost::split(tok, msg, is_any_of(", \t"), token_compress_on);
      if (tok.size() != 5) continue;

      double integral = atof(tok[0].c_str());int a=atoi(tok[1].c_str()), b=atoi(tok[2].c_str()), c=atoi(tok[3].c_str()), d=atoi(tok[4].c_str());

      if(a==b&&b==c&&c==d&&d==0) {
        coreE = integral;
      } else if (b==c&&c==d&&d==0) {
        continue;//orbital energy
      } else if (c==d&&d==0) {
        I1(2*(a-1),2*(b-1)) = integral; //alpha,alpha
        I1(2*(a-1)+1,2*(b-1)+1) = integral; //beta,beta
        I1(2*(b-1),2*(a-1)) = integral; //alpha,alpha
        I1(2*(b-1)+1,2*(a-1)+1) = integral; //beta,beta
      } else {
        I2(2*(a-1),2*(b-1),2*(c-1),2*(d-1)) = integral;
      }
    } // while

    //exit(0);
    I2.maxEntry = *std::max_element(&I2.store[0], &I2.store[0]+I2memory,myfn);
    I2.Direct = MatrixXd::Zero(norbs, norbs); I2.Direct *= 0.;
    I2.Exchange = MatrixXd::Zero(norbs, norbs); I2.Exchange *= 0.;

    for (int i=0; i<norbs; i++)
      for (int j=0; j<norbs; j++) {
        I2.Direct(i,j) = I2(2*i,2*i,2*j,2*j);
        I2.Exchange(i,j) = I2(2*i,2*j,2*j,2*i);
    }

  } // commrank=0

#ifndef SERIAL
  mpi::broadcast(world, I1, 0);

  long intdim = I2memory;
  long  maxint = 26843540; //mpi cannot transfer more than these number of doubles
  long maxIter = intdim/maxint;

  world.barrier();
  for (int i=0; i<maxIter; i++) {
    MPI::COMM_WORLD.Bcast(&I2.store[i*maxint], maxint, MPI_DOUBLE, 0);
    world.barrier();
  }
  MPI::COMM_WORLD.Bcast(&I2.store[(maxIter)*maxint], I2memory - maxIter*maxint, MPI_DOUBLE, 0);
  world.barrier();

  mpi::broadcast(world, I2.maxEntry, 0);
  mpi::broadcast(world, I2.Direct, 0);
  mpi::broadcast(world, I2.Exchange, 0);
  mpi::broadcast(world, I2.zero, 0);
  mpi::broadcast(world, coreE, 0);
#endif

  Determinant::EffDetLen = (norbs) / 64 + 1;
  Determinant::norbs = norbs;
  Determinant::nalpha = nalpha;
  Determinant::nbeta = nbeta;

  //initialize the heatbath integrals
  std::vector<int> allorbs;
  for (int i = 0; i < norbs; i++)
    allorbs.push_back(i);
  twoIntHeatBath I2HB(1.e-10);

  if (commrank == 0)
    I2HB.constructClass(allorbs, I2, I1, norbs);
  I2hb.constructClass(norbs, I2HB);

} // end readIntegrals




//=============================================================================
int readNorbs(string fcidump) {
//-----------------------------------------------------------------------------
    /*!
    Finds the number of orbitals in the FCIDUMP file

    :Inputs:

        string fcidump:
            Name of the FCIDUMP file

    :Returns:

        int norbs:
            Number of orbitals in the FCIDUMP file
    */
//-----------------------------------------------------------------------------
#ifndef SERIAL
  boost::mpi::communicator world;
#endif
  int norbs;
  if (commrank == 0) {
    ifstream dump(fcidump.c_str());
    vector<string> tok;
    string msg;

    std::getline(dump, msg);
    trim(msg);
    boost::split(tok, msg, is_any_of(", \t="), token_compress_on);

    if (boost::iequals(tok[0].substr(0,4),"&FCI"))
      if (boost::iequals(tok[1].substr(0,4), "NORB"))
        norbs = atoi(tok[2].c_str());
  }
#ifndef SERIAL
  mpi::broadcast(world, norbs, 0);
#endif
  return norbs;
} // end readNorbs


//=============================================================================
void twoIntHeatBathSHM::constructClass(int norbs, twoIntHeatBath& I2) {
//-----------------------------------------------------------------------------
    /*!
    BM_description

    :Inputs:

        int norbs:
            Number of orbitals
        twoIntHeatBath& I2:
            Two-electron tensor of the Hamiltonian (output)
    */
//-----------------------------------------------------------------------------
#ifndef SERIAL
  boost::mpi::communicator world;
#endif

  sameSpinPairExcitations = MatrixXd::Zero(norbs, norbs);
  oppositeSpinPairExcitations = MatrixXd::Zero(norbs, norbs);

  Singles = I2.Singles;
  if (commrank != 0) Singles.resize(2*norbs, 2*norbs);

#ifndef SERIAL
  MPI::COMM_WORLD.Bcast(&Singles(0,0), Singles.rows()*Singles.cols(), MPI_DOUBLE, 0);
#endif
  

  I2.Singles.resize(0,0);
  size_t memRequired = 0;
  size_t nonZeroSameSpinIntegrals = 0;
  size_t nonZeroOppositeSpinIntegrals = 0;
  size_t nonZeroSingleExcitationIntegrals = 0;

  if (commrank == 0) {
    std::map<std::pair<short,short>, std::multimap<float, std::pair<short,short>, compAbs > >::iterator it1 = I2.sameSpin.begin();
    for (;it1!= I2.sameSpin.end(); it1++)  nonZeroSameSpinIntegrals += it1->second.size();
      
    std::map<std::pair<short,short>, std::multimap<float, std::pair<short,short>, compAbs > >::iterator it2 = I2.oppositeSpin.begin();
    for (;it2!= I2.oppositeSpin.end(); it2++) nonZeroOppositeSpinIntegrals += it2->second.size();

    std::map<std::pair<short,short>, std::multimap<float, short, compAbs > >::iterator it3 = I2.singleIntegrals.begin();
    for (;it3!= I2.singleIntegrals.end(); it3++) nonZeroSingleExcitationIntegrals += it3->second.size();

    //total Memory required
    memRequired += nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))+ ( (norbs*(norbs+1)/2+1)*sizeof(size_t));
    memRequired += nonZeroOppositeSpinIntegrals*(sizeof(float)+2*sizeof(short))+ ( (norbs*(norbs+1)/2+1)*sizeof(size_t));
    memRequired += nonZeroSingleExcitationIntegrals*(sizeof(float)+2*sizeof(short))+ ( (norbs*(norbs+1)/2+1)*sizeof(size_t));
  }

#ifndef SERIAL
  mpi::broadcast(world, memRequired, 0);
  mpi::broadcast(world, nonZeroSameSpinIntegrals, 0);
  mpi::broadcast(world, nonZeroOppositeSpinIntegrals, 0);
  mpi::broadcast(world, nonZeroSingleExcitationIntegrals, 0);
  world.barrier();
#endif

  int2SHMSegment.truncate(memRequired);
  regionInt2SHM = boost::interprocess::mapped_region{int2SHMSegment, boost::interprocess::read_write};
  memset(regionInt2SHM.get_address(), 0., memRequired);

#ifndef SERIAL
  world.barrier();
#endif

  char* startAddress = (char*)(regionInt2SHM.get_address());
  sameSpinIntegrals           = (float*)(startAddress);
  startingIndicesSameSpin     = (size_t*)(startAddress
                              + nonZeroSameSpinIntegrals*sizeof(float));
  sameSpinPairs               = (short*)(startAddress
                              + nonZeroSameSpinIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t));

  oppositeSpinIntegrals       = (float*)(startAddress
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t));
  startingIndicesOppositeSpin = (size_t*)(startAddress
                              + nonZeroOppositeSpinIntegrals*sizeof(float)
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t));
  oppositeSpinPairs             = (short*)(startAddress
                              + nonZeroOppositeSpinIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t));

  singleIntegrals             = (float*)(startAddress
                              + nonZeroOppositeSpinIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
			      + nonZeroOppositeSpinIntegrals*(2*sizeof(short)));
  startingIndicesSingleIntegrals = (size_t*)(startAddress
                              + nonZeroOppositeSpinIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
			      + nonZeroOppositeSpinIntegrals*(2*sizeof(short))
			      + nonZeroSingleExcitationIntegrals*sizeof(float));
  singleIntegralsPairs        = (short*)(startAddress
                              + nonZeroOppositeSpinIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
                              + nonZeroSameSpinIntegrals*(sizeof(float)+2*sizeof(short))
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t)
			      + nonZeroOppositeSpinIntegrals*(2*sizeof(short))
			      + nonZeroSingleExcitationIntegrals*sizeof(float)
                              + (norbs*(norbs+1)/2+1)*sizeof(size_t));

  if (commrank == 0) {
    startingIndicesSameSpin[0] = 0;
    size_t index = 0, pairIter = 1;
    for (int i=0; i<norbs; i++)
      for (int j=0; j<=i; j++) {
        std::map<std::pair<short,short>, std::multimap<float, std::pair<short,short>, compAbs > >::iterator it1 = I2.sameSpin.find( std::pair<short,short>(i,j));

        if (it1 != I2.sameSpin.end()) {
          for (std::multimap<float, std::pair<short,short>,compAbs >::reverse_iterator it=it1->second.rbegin(); it!=it1->second.rend(); it++) {
            sameSpinIntegrals[index] = it->first;
            sameSpinPairs[2*index] = it->second.first;
            sameSpinPairs[2*index+1] = it->second.second;
	    sameSpinPairExcitations(it->second.first, it->second.second) += abs(it->first); 

            index++;
          }
        }
        startingIndicesSameSpin[pairIter] = index;

        pairIter++;
    }
    I2.sameSpin.clear();

    startingIndicesOppositeSpin[0] = 0;
    index = 0; pairIter = 1;
    for (int i=0; i<norbs; i++)
      for (int j=0; j<=i; j++) {
        std::map<std::pair<short,short>, std::multimap<float, std::pair<short,short>, compAbs > >::iterator it1 = I2.oppositeSpin.find( std::pair<short,short>(i,j));

        if (it1 != I2.oppositeSpin.end()) {
          for (std::multimap<float, std::pair<short,short>,compAbs >::reverse_iterator it=it1->second.rbegin(); it!=it1->second.rend(); it++) {
            oppositeSpinIntegrals[index] = it->first;
            oppositeSpinPairs[2*index] = it->second.first;
            oppositeSpinPairs[2*index+1] = it->second.second;
	    oppositeSpinPairExcitations(it->second.first, it->second.second) += abs(it->first); 
            index++;
          }
        }
        startingIndicesOppositeSpin[pairIter] = index;
        pairIter++;
    }
    I2.oppositeSpin.clear();

    startingIndicesSingleIntegrals[0] = 0;
    index = 0; pairIter = 1;
    for (int i=0; i<norbs; i++)
      for (int j=0; j<=i; j++) {
        std::map<std::pair<short,short>, std::multimap<float, short, compAbs > >::iterator it1 = I2.singleIntegrals.find( std::pair<short,short>(i,j));

        if (it1 != I2.singleIntegrals.end()) {
          for (std::multimap<float, short,compAbs >::reverse_iterator it=it1->second.rbegin(); it!=it1->second.rend(); it++) {
            singleIntegrals[index] = it->first;
            singleIntegralsPairs[2*index] = it->second;
            index++;
          }
        }
        startingIndicesSingleIntegrals[pairIter] = index;
        pairIter++;
    }
    I2.singleIntegrals.clear();



  } // commrank=0

  long intdim = memRequired;
  long  maxint = 26843540; //mpi cannot transfer more than these number of doubles
  long maxIter = intdim/maxint;
#ifndef SERIAL
  world.barrier();
  char* shrdMem = static_cast<char*>(startAddress);
  for (int i=0; i<maxIter; i++) {
    MPI::COMM_WORLD.Bcast(shrdMem+i*maxint, maxint, MPI_CHAR, 0);
    world.barrier();
  }
  MPI::COMM_WORLD.Bcast(shrdMem+(maxIter)*maxint, memRequired - maxIter*maxint, MPI_CHAR, 0);
  world.barrier();
#endif

#ifndef SERIAL
  MPI::COMM_WORLD.Bcast(&sameSpinPairExcitations(0,0), sameSpinPairExcitations.rows()*
			sameSpinPairExcitations.cols(), MPI_DOUBLE, 0);
  MPI::COMM_WORLD.Bcast(&oppositeSpinPairExcitations(0,0), oppositeSpinPairExcitations.rows()*
			oppositeSpinPairExcitations.cols(), MPI_DOUBLE, 0);
#endif

} // end twoIntHeatBathSHM::constructClass

