/*
  Copyright 2024-2025, Mikhail Grushinskiy
*/

#define EIGEN_NO_DEBUG
#define EIGEN_NON_ARDUINO

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "DirectionalSpread.h"
#include "FentonWaveVectorized.h"
#include "Jonswap3dStokesWaves.h"
#include "PiersonMoskowitzStokes3D_Waves.h"

namespace {

struct TestFailure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

bool approx_equal(double lhs, double rhs, double rel_tol = 1e-6, double abs_tol = 1e-8) {
  const double diff = std::abs(lhs - rhs);
  const double scale = std::max(std::abs(lhs), std::abs(rhs));
  return diff <= std::max(abs_tol, rel_tol * scale);
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    throw TestFailure(message);
  }
}

void require_near(double lhs, double rhs, double rel_tol, double abs_tol, const std::string &message) {
  if (!approx_equal(lhs, rhs, rel_tol, abs_tol)) {
    std::ostringstream oss;
    oss << message << " (lhs=" << lhs << ", rhs=" << rhs << ")";
    throw TestFailure(oss.str());
  }
}

void require_finite(double value, const std::string &message) {
  if (!std::isfinite(value)) {
    std::ostringstream oss;
    oss << message << " (value=" << value << ")";
    throw TestFailure(oss.str());
  }
}

void test_directional_distribution_normalization() {
  Cosine2sRandomizedDistribution distribution(M_PI / 6.0, 8.0, 1234u);
  const int bins = 360;
  const double dtheta = 2.0 * PI / bins;
  const auto weights = distribution.normalized_weights(bins, 0.2);

  require(static_cast<int>(weights.size()) == bins, "directional weights should match requested bins");

  double integral = 0.0;
  for (int i = 0; i < bins; ++i) {
    const double factor = (i == 0 || i == bins - 1) ? 0.5 : 1.0;
    require(weights[i] >= 0.0, "directional weights should be non-negative");
    integral += factor * weights[i];
  }
  integral *= dtheta;

  require_near(integral, 1.0, 1e-3, 1e-3, "directional weights should integrate to one");
  require_near(distribution.principal_direction_rad(), M_PI / 6.0, 1e-12, 1e-12,
               "principal direction should be preserved");
}

void test_fenton_wave_invariants() {
  constexpr double height = 2.0;
  constexpr double depth = 10.0;
  constexpr double length = 50.0;

  FentonWave<5, float> wave(static_cast<float>(height), static_cast<float>(depth), static_cast<float>(length));

  require_near(2.0 * M_PI / wave.get_k(), length, 5e-3, 5e-3,
               "Fenton wavelength should match the configured length");
  require_near(wave.get_length(), length, 1e-10, 1e-10,
               "Fenton wave should report the configured length");
  require_near(wave.get_height(), height, 1e-10, 1e-10,
               "Fenton wave should report the configured height");

  const double x = 7.5;
  const double t = 1.2;
  const double eta = wave.surface_elevation(x, t);
  const double eta_periodic = wave.surface_elevation(x + wave.get_length(), t);
  const double eta_slope = wave.surface_slope(x, t);
  const double eta_t = wave.surface_time_derivative(x, t);

  require_finite(eta, "Fenton surface elevation should be finite");
  require_finite(eta_slope, "Fenton surface slope should be finite");
  require_finite(eta_t, "Fenton time derivative should be finite");
  require_near(eta_periodic, eta, 1e-6, 1e-6,
               "Fenton surface elevation should be periodic over one wavelength");
  require_near(eta_t, -wave.get_c() * eta_slope, 1e-5, 1e-5,
               "Fenton surface time derivative should satisfy the traveling-wave relation");

  double min_eta = wave.surface_elevation(0.0, 0.0);
  double max_eta = min_eta;
  constexpr int samples = 400;
  for (int i = 0; i <= samples; ++i) {
    const double xi = length * static_cast<double>(i) / samples;
    const double elev = wave.surface_elevation(xi, 0.0);
    min_eta = std::min(min_eta, elev);
    max_eta = std::max(max_eta, elev);
  }
  require_near(max_eta - min_eta, height, 5e-2, 5e-2,
               "Fenton crest-to-trough span should approximate the configured height");
}

void test_jonswap_spectrum_and_state() {
  constexpr double hs = 2.0;
  constexpr double tp = 7.0;
  auto distribution = std::make_shared<Cosine2sRandomizedDistribution>(0.0, 12.0, 321u);

  JonswapSpectrum<128> spectrum(hs, tp);
  require_near(spectrum.integratedVariance(), (hs * hs) / 16.0, 5e-3, 5e-4,
               "JONSWAP spectrum variance should match Hs");
  require_near(spectrum.gamma(), 3.3, 1e-12, 1e-12,
               "JONSWAP spectrum should preserve default gamma");

  const auto &freqs = spectrum.frequencies();
  const auto &amps = spectrum.amplitudes();
  for (int i = 0; i < freqs.size(); ++i) {
    require_finite(freqs(i), "JONSWAP frequencies should be finite");
    require_finite(amps(i), "JONSWAP amplitudes should be finite");
    require(amps(i) >= 0.0, "JONSWAP amplitudes should be non-negative");
    if (i > 0) {
      require(freqs(i) > freqs(i - 1), "JONSWAP frequencies should be strictly increasing");
    }
  }

  Jonswap3dStokesWaves<64> wave_a(hs, tp, distribution, 0.02, 0.8, 3.3, 9.81, 42u);
  Jonswap3dStokesWaves<64> wave_b(hs, tp, std::make_shared<Cosine2sRandomizedDistribution>(0.0, 12.0, 321u),
                                  0.02, 0.8, 3.3, 9.81, 42u);

  const auto state_a = wave_a.getLagrangianState(1.0, -0.5, 3.25, -0.75);
  const auto state_b = wave_b.getLagrangianState(1.0, -0.5, 3.25, -0.75);
  const auto slopes = wave_a.getSurfaceSlopes(0.5, 1.5, 2.0);

  for (int i = 0; i < 3; ++i) {
    require_finite(state_a.displacement(i), "JONSWAP displacement should be finite");
    require_finite(state_a.velocity(i), "JONSWAP velocity should be finite");
    require_finite(state_a.acceleration(i), "JONSWAP acceleration should be finite");
    require_near(state_a.displacement(i), state_b.displacement(i), 1e-10, 1e-10,
                 "JONSWAP seeded displacement should be deterministic");
    require_near(state_a.velocity(i), state_b.velocity(i), 1e-10, 1e-10,
                 "JONSWAP seeded velocity should be deterministic");
    require_near(state_a.acceleration(i), state_b.acceleration(i), 1e-10, 1e-10,
                 "JONSWAP seeded acceleration should be deterministic");
  }
  require_finite(slopes.x(), "JONSWAP surface slope x should be finite");
  require_finite(slopes.y(), "JONSWAP surface slope y should be finite");
}

void test_pm_spectrum_and_errors() {
  constexpr double hs = 2.5;
  constexpr double tp = 8.0;

  PiersonMoskowitzSpectrum<128> spectrum(hs, tp);
  require_near(spectrum.integratedVariance(), (hs * hs) / 16.0, 5e-3, 5e-4,
               "PM spectrum variance should match Hs");

  const auto distribution = std::make_shared<Cosine2sRandomizedDistribution>(M_PI / 8.0, 10.0, 777u);
  PMStokesN3dWaves<64, 3> wave(hs, tp, distribution, 0.02, 0.8, 9.81, 99u);
  const auto state = wave.getEulerianState(1.0, -2.0, -0.5, 4.0);
  const auto slopes = wave.getSurfaceSlopes(1.0, -2.0, 4.0);

  for (int i = 0; i < 3; ++i) {
    require_finite(state.displacement(i), "PM displacement should be finite");
    require_finite(state.velocity(i), "PM velocity should be finite");
    require_finite(state.acceleration(i), "PM acceleration should be finite");
  }
  require_finite(slopes.x(), "PM surface slope x should be finite");
  require_finite(slopes.y(), "PM surface slope y should be finite");

  bool threw = false;
  try {
    JonswapSpectrum<32> invalid_spectrum(-1.0, 7.0);
    (void)invalid_spectrum;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  require(threw, "JONSWAP spectrum should reject non-positive Hs");

  threw = false;
  try {
    PMStokesN3dWaves<32, 3> invalid_wave(hs, tp, nullptr);
    (void)invalid_wave;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  require(threw, "PM Stokes waves should reject a null directional distribution");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"directional distribution normalization", test_directional_distribution_normalization},
      {"Fenton wave invariants", test_fenton_wave_invariants},
      {"JONSWAP spectrum and state", test_jonswap_spectrum_and_state},
      {"Pierson-Moskowitz spectrum and error handling", test_pm_spectrum_and_errors},
  };

  int failures = 0;
  for (const auto &[name, test] : tests) {
    try {
      test();
      std::cout << "[PASS] " << name << '\n';
    } catch (const std::exception &ex) {
      ++failures;
      std::cerr << "[FAIL] " << name << ": " << ex.what() << '\n';
    }
  }

  if (failures != 0) {
    std::cerr << failures << " test case(s) failed\n";
    return 1;
  }

  std::cout << "All " << tests.size() << " test cases passed\n";
  return 0;
}
