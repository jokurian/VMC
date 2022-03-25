//#define EIGEN_USE_MKL_ALL
#include <iostream>
#ifndef SERIAL
#include "mpi.h"
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/mpi.hpp>
#endif
#include "input.h"
#include "integral.h"
#include "SHCIshm.h"
//#include "DQMCSampling.h"
//#include "ProjectedMF.h"
//#include "RHF.h"
#include "UHF.h"
//#include "KSGHF.h"
#include "Multislater.h"
//#include "CCSD.h"
//#include "UCCSD.h"
//#include "sJastrow.h"
#include "MixedEstimator.h"

int main(int argc, char *argv[])
{

#ifndef SERIAL
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;
#endif
  startofCalc = getTime();

  bool print = true;
  initSHM();
  //license();
  if (commrank == 0 && print) {
    std::system("echo User:; echo $USER");
    std::system("echo Hostname:; echo $HOSTNAME");
    std::system("echo CPU info:; lscpu | head -15");
    std::system("echo Computation started at:; date");
    cout << "git commit: " << GIT_HASH << ", branch: " << GIT_BRANCH << ", compiled at: " << COMPILE_TIME << endl << endl;
    cout << "nproc used: " << commsize << " (NB: stochasticIter below is per proc)" << endl << endl; 
  }

  string inputFile = "input.dat";
  if (argc > 1)
    inputFile = string(argv[1]);
  readInput(inputFile, schd, print);

  generator = std::mt19937(schd.seed + commrank);

  //MatrixXd h1, h1Mod;
  //vector<MatrixXd> chol;
  //int norbs, nalpha, nbeta;
  //double ecore;
  //readIntegralsCholeskyAndInitializeDeterminantStaticVariables(schd.integralsFile, norbs, nalpha, nbeta, ecore, h1, h1Mod, chol);
  
  Hamiltonian ham(schd.integralsFile);
  if (commrank == 0) cout << "Number of orbitals:  " << ham.norbs << ", nalpha:  " << ham.nalpha << ", nbeta:  " << ham.nbeta << endl;
  DQMCWalker walker;
  walker = DQMCWalker(false, true);
  
  //walker = DQMCWalker(false, true);
  //std::array<Eigen::MatrixXcd, 2> det;
  //det[0] = Eigen::MatrixXcd::Zero(4, 2);
  //det[1] = Eigen::MatrixXcd::Zero(4, 2);
  //readMat(det[0], "up.dat");
  //readMat(det[1], "down.dat");
  //cout << "up\n" << det[0] << endl << endl;
  //cout << "dn\n" << det[1] << endl << endl;
  //walker.setDet(det);

  // left state
  Wavefunction *waveLeft;
  if (schd.leftWave == "uhf") {
    waveLeft = new UHF(ham, true); 
  }
  else if (schd.leftWave == "multislater") {
    int nact = (schd.nciAct < 0) ? ham.norbs : schd.nciAct;
    waveLeft = new Multislater(schd.determinantFile, nact, schd.nciCore); 
  }
  //auto overlap = walker.overlap(*waveLeft);
  //Eigen::VectorXcd fb;
  //walker.forceBias(*waveLeft, ham, fb);
  //Eigen::MatrixXcd rdmSample;
  //walker.oneRDM(*waveLeft, rdmSample);
  //auto hamOverlap = walker.hamAndOverlap(*waveLeft, ham);
  //cout << "overlap:  " << overlap << endl << endl;
  //cout << "fb\n" << fb << endl << endl;
  //cout << "rdmSample\n" << rdmSample << endl << endl;
  //cout << "ham:  " << hamOverlap[0] << ",  overlap:  " <<  hamOverlap[1] << endl << endl;
  //cout << "eloc:  " << hamOverlap[0] / hamOverlap[1] << endl;
  //exit(0);

  // right state
  Wavefunction *waveRight;
  waveRight = new UHF(ham, false); 

  Wavefunction *waveGuide;
  waveGuide = new UHF(ham, false); 
  calcMixedEstimatorLongProp(*waveLeft, *waveRight, *waveGuide, walker, ham);
  
  //else calcMixedEstimatorLongProp(*waveLeft, *waveRight, walker, ham);
  
  if (commrank == 0) cout << "\nTotal calculation time:  " << getTime() - startofCalc << " s\n";
  boost::interprocess::shared_memory_object::remove(shciint2.c_str());
  boost::interprocess::shared_memory_object::remove(shciint2shm.c_str());
  boost::interprocess::shared_memory_object::remove(shciint2shmcas.c_str());
  return 0;
}
