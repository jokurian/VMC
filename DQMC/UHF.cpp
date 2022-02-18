#include "input.h"
#include "UHF.h"

using namespace std;
using namespace Eigen;

using matPair = std::array<MatrixXcd, 2>;


UHF::UHF(Hamiltonian& ham, bool pleftQ, std::string fname) 
{
  MatrixXd hf = MatrixXd::Zero(ham.norbs, 2*ham.norbs);
  readMat(hf, fname);
  det[0] = hf.block(0, 0, ham.norbs, ham.nalpha);
  det[1] = hf.block(0, ham.norbs, ham.norbs, ham.nbeta);
  detT[0] = det[0].adjoint();
  detT[1] = det[1].adjoint();
  leftQ = pleftQ;
  if (leftQ) {
    ham.rotateCholesky(detT, rotChol);
  }
};


void UHF::getSample(std::array<Eigen::MatrixXcd, 2>& sampleDet) 
{
  sampleDet[0] = det[0];
  sampleDet[1] = det[1];
};


std::complex<double> UHF::overlap(std::array<Eigen::MatrixXcd, 2>& psi)
{
  complex<double> overlap = (detT[0] * psi[0]).determinant() * (detT[1] * psi[1]).determinant();
  return overlap;
};


void UHF::forceBias(std::array<Eigen::MatrixXcd, 2>& psi, Hamiltonian& ham, Eigen::VectorXcd& fb)
{
  matPair thetaT;
  thetaT[0] = (psi[0] * (detT[0] * psi[0]).inverse()).transpose();
  thetaT[1] = (psi[1] * (detT[1] * psi[1]).inverse()).transpose();
  fb = VectorXcd::Zero(rotChol.size());
  for (int i = 0; i < rotChol.size(); i++) {
    fb(i) = thetaT[0].cwiseProduct(rotChol[i][0]).sum() + thetaT[1].cwiseProduct(rotChol[i][1]).sum();
  }
};

void UHF::oneRDM(std::array<Eigen::MatrixXcd, 2>& det, std::array<Eigen::MatrixXcd, 2>& rdmSample) 
{
  rdmSample[0] = (det[0] * (detT[0] * det[0]).inverse() * detT[0]).transpose();
  rdmSample[1] = (det[1] * (detT[1] * det[1]).inverse() * detT[1]).transpose();
};

std::array<std::complex<double>, 2> UHF::hamAndOverlap(std::array<Eigen::MatrixXcd, 2>& psi, Hamiltonian& ham) 
{ 
  complex<double> overlap = (detT[0] * psi[0]).determinant() * (detT[1] * psi[1]).determinant();
  complex<double> ene = ham.ecore;
  
  // calculate theta and green
  matPair theta, green;
  theta[0] = psi[0] * (detT[0] * psi[0]).inverse();
  theta[1] = psi[1] * (detT[1] * psi[1]).inverse();
  green[0] = theta[0] * detT[0];
  green[1] = theta[1] * detT[1];

  // one body part
  ene += green[0].cwiseProduct(ham.h1[0]).sum() + green[1].cwiseProduct(ham.h1[1]).sum();

  // two body part
  MatrixXcd fup = MatrixXcd::Zero(rotChol[0][0].rows(), rotChol[0][0].rows());
  MatrixXcd fdn = MatrixXcd::Zero(rotChol[0][1].rows(), rotChol[0][1].rows());
  for (int i = 0; i < ham.nchol; i++) {
    fup.noalias() = rotChol[i][0] * theta[0];
    fdn.noalias() = rotChol[i][1] * theta[1];
    complex<double> cup = fup.trace();
    complex<double> cdn = fdn.trace();
    ene += (cup * cup + cdn * cdn + 2. * cup * cdn - fup.cwiseProduct(fup.transpose()).sum() - fdn.cwiseProduct(fdn.transpose()).sum()) / 2.;
  }

  std::array<complex<double>, 2> hamOverlap;
  hamOverlap[0] = ene * overlap;
  hamOverlap[1] = overlap;
  return hamOverlap;
};
