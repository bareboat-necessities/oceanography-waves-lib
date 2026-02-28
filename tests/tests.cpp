/*
  Copyright 2024-2025, Mikhail Grushinskiy
*/

#define EIGEN_NO_DEBUG
#define EIGEN_NON_ARDUINO
//#define EIGEN_DONT_VECTORIZE
//#define EIGEN_MPL2_ONLY

#define FENTON_TEST
#define JONSWAP_TEST
#define PM_STOKES_TEST

#include <cmath>
#include <random>

#include "TrochoidalWave.h"
#include "FentonWaveVectorized.h"
#include "Jonswap3dStokesWaves.h"
#include "PiersonMoskowitzStokes3D_Waves.h"

int main(int argc, char *argv[]) {

#ifdef PM_STOKES_TEST
  PMStokes_testWavePatterns();
  PMStokes_testWaveSpectrum();
#endif

#ifdef JONSWAP_TEST
  Jonswap_testWavePatterns();
  Jonswap_testWaveSpectrum();
#endif

#ifdef FENTON_TEST
  FentonWave_test_1();
  FentonWave_test_2();
#endif

}