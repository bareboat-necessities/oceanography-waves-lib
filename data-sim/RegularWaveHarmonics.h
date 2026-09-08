#pragma once
#include "WaveHarmonic.h"
#include "TrochoidalWave.h"
#include "FentonWaveVectorized.h"
#include "CnoidalWave.h"
#include "WaveFilesSupport.h"
#include <complex>
#include <array>

// Fourier projection of an existing periodic Eulerian surface profile. The
// zero-frequency elevation is a datum and is excluded from oscillatory motion.
template<class Elevation>
std::vector<WaveHarmonic> periodicWaveHarmonics(double period, double k,
                                              Elevation elevation) {
    constexpr int samples=2048, harmonics=64;
    const double pi=std::acos(-1.0), omega=2*pi/period;
    std::array<double,samples> eta{};
    for (int j=0; j<samples; ++j) eta[j]=elevation(period*j/samples);
    std::vector<WaveHarmonic> result;
    for (int n=1; n<=harmonics; ++n) {
        std::complex<double> a=0;
        for (int j=0; j<samples; ++j)
            a+=eta[j]*std::polar(2.0/samples,2*pi*n*j/samples);
        if (std::abs(a)>1e-12)
            result.push_back({std::abs(a),n*omega,n*k,0.0,std::arg(a)});
    }
    return result;
}

inline std::vector<WaveHarmonic> regularWaveHarmonics(WaveType type, const WaveParameters& wp) {
    const double pi=std::acos(-1.0), omega=2*pi/wp.period;
    if (type==WaveType::GERSTNER) {
        const double a=wp.height/2.0, k=omega*omega/g_std;
        // Invert x=q-a sin(kq-wt+phase) at fixed x=0. This samples the
        // incident surface, not the trajectory of a travelling water particle.
        return periodicWaveHarmonics(wp.period,k,[&](double t) {
            double q=0;
            for (int j=0; j<16; ++j) {
                const double phase=k*q-omega*t+wp.phase;
                q-=(q-a*std::sin(phase))/(1-a*k*std::cos(phase));
            }
            return -a*std::cos(k*q-omega*t+wp.phase);
        });
    }
    if (type==WaveType::FENTON) {
        const auto p=FentonWave<5>::infer_fenton_parameters_from_amplitude(
            wp.height/2.0f,200.0f,2.0f*M_PI/wp.period,wp.phase);
        const FentonWave<5> wave(p.height,p.depth,p.length);
        std::vector<WaveHarmonic> result;
        for (int n=1; n<=5; ++n) {
            const double a=wave.E(n);
            result.push_back({std::abs(a),n*wave.omega,n*wave.k,0.0,
                              n*wave.k*p.initial_x+(a<0 ? pi : 0)});
        }
        return result;
    }
    if (type==WaveType::CNOIDAL) {
        // Keep the existing constructor inputs and implemented profile. The
        // legacy Jacobi helper performs an extra phase halving, so this code's
        // actual repeat period is 8K/omega (mathematical cn would use 4K/omega).
        const CnoidalWave<float> wave(200.0f,wp.height/2.0f,wp.period,0.0f,g_std);
        const double phase_period=8*Elliptic::ellipK(wave.ellipticM());
        return periodicWaveHarmonics(phase_period/wave.frequency(),
            2*pi*wave.wavenumber()/phase_period,[&](double t) {
                return static_cast<double>(wave.surfaceElevation(0,0,static_cast<float>(t)));
            });
    }
    throw std::invalid_argument("Expected a regular-wave type");
}
