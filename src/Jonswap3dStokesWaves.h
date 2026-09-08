#pragma once
#pragma GCC optimize ("no-fast-math")

/*
  Copyright 2025, Mikhail Grushinskiy

  JONSWAP-spectrum second-order deep-water potential-flow waves.
  - Complete quadratic sum/difference interactions and Lagrangian advection.
  - Particle displacement, velocity and acceleration share one trajectory.
  - z=0 labels the surface; x,y,z are particle labels for particle methods.
  - Eulerian surface slopes and particle-following slopes are separate APIs.
  See doc/jonswap-second-order.md for scope and migration details.
*/

#ifdef EIGEN_NON_ARDUINO
#include <Eigen/Dense>
#else
#include <ArduinoEigenDense.h>
#endif

#include <random>
#include <cmath>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <vector>
#include <memory>

#include "DirectionalSpread.h"
#include "DeepWaterSecondOrder.h"

#ifdef JONSWAP_TEST
#include <iostream>
#include <fstream>
#endif

// JonswapSpectrum
template<int N_FREQ = 128>
class EIGEN_ALIGN_MAX JonswapSpectrum {
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    JonswapSpectrum(double Hs, double Tp,
                    double f_min = 0.02, double f_max = 0.8,
                    double gamma = 3.3, double g = 9.81)
      : Hs_(Hs), Tp_(Tp), f_min_(f_min), f_max_(f_max), gamma_(gamma), g_(g)
    {
      if (N_FREQ < 2) throw std::runtime_error("N_FREQ must be >= 2");
      if (!(std::isfinite(g_) && g_ > 0 && std::isfinite(gamma_) && gamma_ > 0))
        throw std::runtime_error("gravity and gamma must be finite and positive");
      if (!(std::isfinite(Hs_) && Hs_ > 0.0)) throw std::runtime_error("Hs must be > 0");
      if (!(std::isfinite(Tp_) && Tp_ > 0.0)) throw std::runtime_error("Tp must be > 0");
      if (!(std::isfinite(f_min_) && std::isfinite(f_max_) && f_min_ > 0.0 && f_max_ > f_min_)) throw std::runtime_error("Invalid frequency range");
      if (!((1.0 / Tp_) >= f_min_ && (1.0 / Tp_) <= f_max_))
        throw std::runtime_error("1/Tp must be within [f_min, f_max]");

      frequencies_.setZero(); S_.setZero(); A_.setZero(); df_.setZero();

      computeLogFrequencySpacing();
      computeFrequencyIncrements();
      computeJonswapSpectrumFromHs();
    }

    const Eigen::Matrix<double, N_FREQ, 1>& frequencies() const {
      return frequencies_;
    }
    const Eigen::Matrix<double, N_FREQ, 1>& spectrum() const {
      return S_;
    }
    const Eigen::Matrix<double, N_FREQ, 1>& amplitudes() const {
      return A_;
    }
    const Eigen::Matrix<double, N_FREQ, 1>& df() const {
      return df_;
    }
    double integratedVariance() const {
      return (S_.cwiseProduct(df_)).sum();
    }
    double gamma() const { return gamma_; }

  private:
    double Hs_, Tp_, f_min_, f_max_, gamma_, g_;
    Eigen::Matrix<double, N_FREQ, 1> frequencies_, S_, A_, df_;

    void computeLogFrequencySpacing() {
      const double log_f_min = std::log(f_min_);
      const double log_f_max = std::log(f_max_);
      Eigen::ArrayXd idx = Eigen::ArrayXd::LinSpaced(N_FREQ, 0, N_FREQ - 1);
      frequencies_ = (log_f_min + (log_f_max - log_f_min) * idx / (N_FREQ - 1)).exp().matrix();
    }

    void computeFrequencyIncrements() {
      df_.head(N_FREQ - 1) = (frequencies_.segment(1, N_FREQ - 1) - frequencies_.head(N_FREQ - 1));
      df_.segment(1, N_FREQ - 2) = 0.5 * (frequencies_.segment(2, N_FREQ - 2) - frequencies_.head(N_FREQ - 2));
      df_(N_FREQ - 1) = frequencies_(N_FREQ - 1) - frequencies_(N_FREQ - 2);
    }

    void computeJonswapSpectrumFromHs() {
        const double fp = 1.0 / Tp_;
        Eigen::Matrix<double, N_FREQ, 1> S0;

        for (int i = 0; i < N_FREQ; ++i) {
            const double f = frequencies_(i);
            const double sigma = (f <= fp) ? 0.07 : 0.09;
            const double dfreq = f - fp;
            const double denom = 2.0 * sigma * sigma * fp * fp;
            const double r = std::exp(-(dfreq * dfreq) / denom);

            const double f2 = f * f;
            const double f4 = f2 * f2;
            const double inv_f5 = 1.0 / (f * f4);

            const double fp2 = fp * fp;
            const double ratio2 = fp2 / f2;
            const double ratio4 = ratio2 * ratio2;

            const double base = (g_ * g_) / std::pow(2.0 * M_PI, 4) * inv_f5 * std::exp(-1.25 * ratio4);
            const double gamma_r = std::exp(r * std::log(gamma_));
            const double val = base * gamma_r;

            S0(i) = std::isfinite(val) ? val : 0.0;
        }

        const double variance_unit = (S0.cwiseProduct(df_)).sum();
        if (!(variance_unit > 0.0))
            throw std::runtime_error("JonswapSpectrum: computed zero/negative variance");

        const double variance_target = (Hs_ * Hs_) / 16.0;
        const double alpha = variance_target / variance_unit;

        // Initial scaled spectrum
        S_ = S0 * alpha;
        A_ = (2.0 * S_.cwiseProduct(df_)).cwiseSqrt();

        // Enforce Hs by bisection
        auto m0_from_beta = [&](double beta) {
            Eigen::Matrix<double, N_FREQ, 1> A_beta = beta * A_;
            double sum_sq = A_beta.squaredNorm();
            return 0.5 * sum_sq; // variance
        };

        const double m0_target = variance_target;
        const double m0_initial = m0_from_beta(1.0);

        if (m0_initial > m0_target * 1.0001) {
            double lo = 0.0, hi = 1.0;
            for (int it = 0; it < 60; ++it) {
                double mid = 0.5 * (lo + hi);
                if (m0_from_beta(mid) > m0_target) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            double beta = 0.5 * (lo + hi);
            A_ *= beta;
            for (int i = 0; i < N_FREQ; ++i) {
                const double dfi = df_(i) > 0.0 ? df_(i) : 1e-12;
                S_(i) = (A_(i) * A_(i)) / (2.0 * dfi);
            }
        }
    }
};

struct IMUReadings {
  Eigen::Vector3d accel_body;  // specific force in IMU frame (acceleration minus gravity)
  Eigen::Vector3d gyro_body;   // angular velocity in IMU frame (rad/s)
  Eigen::Vector3d accel_debug; // linear acceleration for debugging AHRS filters
};

// Jonswap3dStokesWaves
template<int N_FREQ = 128>
class EIGEN_ALIGN_MAX Jonswap3dStokesWaves {
  public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    struct EIGEN_ALIGN_MAX WaveState {
      Eigen::Vector3d displacement, velocity, acceleration;
    };

    Jonswap3dStokesWaves(double Hs, double Tp,
                         std::shared_ptr<DirectionalDistribution> dirDist,
                         double f_min = 0.02, double f_max = 0.8,
                         double gamma = 3.3, double g = 9.81,
                         unsigned int seed = 42u,
                         double cutoff_tol = 1e-8)
      : spectrum_(Hs,Tp,f_min,f_max,gamma,g),
        directional_dist_(std::move(dirDist)), g_(g),
        model_(makeComponents(spectrum_,directional_dist_,seed,g),g)
    {
      // Retained for source compatibility. No quadratic interactions are pruned:
      // displacement-only cutoffs do not control acceleration error.
      if (!(std::isfinite(cutoff_tol) && cutoff_tol >= 0))
        throw std::invalid_argument("cutoff_tol must be finite and nonnegative");
      const auto steepness=(spectrum_.amplitudes().array()*
          (2*PI*spectrum_.frequencies().array()).square()/g_).eval();
      if (steepness.maxCoeff()>0.4)
        throw std::runtime_error("Jonswap3dStokesWaves: component too steep (>0.4)");
    }

    // Backward-compatible alias: a surface PARTICLE state, not Eulerian eta.
    WaveState getSurfaceState(double x, double y, double t) const {
      return getLagrangianState(x,y,t,0);
    }

    WaveState getLagrangianState(double x, double y, double t, double z=0) const {
      const auto s=model_.state(x,y,t,z);
      return {s.displacement,s.velocity,s.acceleration};
    }

    double getSurfaceElevation(double x, double y, double t) const {
      return model_.surfaceElevation(x,y,t);
    }

    // Eulerian spatial gradient at a fixed horizontal coordinate.
    Eigen::Vector2d getSurfaceSlopes(double x, double y, double t) const {
      return model_.surfaceSlopes(x,y,t);
    }

    // Gradient seen by the labelled particle, consistently through order a^2.
    Eigen::Vector2d getLagrangianSurfaceSlopes(double x, double y, double t) const {
      return model_.particleSurfaceSlopes(x,y,t);
    }

    double linearRmsSlope() const {
      return std::sqrt(0.5*(spectrum_.amplitudes().array().square()*
          (2*PI*spectrum_.frequencies().array()).pow(4)/(g_*g_)).sum());
    }

// build local wave IMU orientation from slopes
// This is a TRUE tilt-only, yaw-fixed frame:
//   - body z aligns with the surface normal
//   - yaw is defined to be zero in the world frame
//   - roll/pitch are exactly the same convention used by getEulerAngles()
Eigen::Matrix3d orientationFromSlopes(const Eigen::Vector2d &slopes) const {
  const double sx = slopes.x();
  const double sy = slopes.y();

  // Tilt-only ZYX convention with yaw = 0:
  //   pitch = atan(-sx)
  //   roll  = atan2(sy, sqrt(1 + sx^2))
  const double pitch = std::atan(-sx);
  const double roll  = std::atan2(sy, std::sqrt(1.0 + sx * sx));

  const double cp = std::cos(pitch);
  const double sp = std::sin(pitch);
  const double cr = std::cos(roll);
  const double sr = std::sin(roll);

  // body -> world with yaw = 0 is:
  //   C_bw = Ry(pitch) * Rx(roll)
  //
  // We return world -> body:
  //   C_wb = C_bw^T
  Eigen::Matrix3d C_wb;
  C_wb <<
      cp,         0.0,    -sp,
      sp * sr,    cr,      cp * sr,
      sp * cr,   -sr,      cp * cr;

  return C_wb;
}

    Eigen::Vector3d getEulerAngles(double x, double y, double t) const {
        const Eigen::Vector2d slopes = getLagrangianSurfaceSlopes(x,y,t);
        const double sx = slopes.x();
        const double sy = slopes.y();

        // World-frame yaw is defined as zero.
        // With z_b aligned to the surface normal n ~ (-sx, -sy, 1),
        // the corresponding roll/pitch (ZYX convention) are:
        //   pitch = atan(-sx)
        //   roll  = atan2(sy, sqrt(1 + sx^2))
        const double pitch = std::atan(-sx);
        const double roll  = std::atan2(sy, std::sqrt(1.0 + sx * sx));
        const double yaw   = 0.0;

        return Eigen::Vector3d(
            roll  * 180.0 / M_PI,
            pitch * 180.0 / M_PI,
            yaw   * 180.0 / M_PI
        );
    }

IMUReadings getIMUReadings(double x, double y, double t,
                           double z = 0.0, double dt = 1e-3) const {
    if (!(std::isfinite(dt) && dt > 0))
      throw std::invalid_argument("IMU differentiation dt must be finite and positive");
    IMUReadings imu;
    (void)z; // surface sensor: translational kinematics also taken at z = 0

    // Surface translational kinematics
    const auto state = getLagrangianState(x, y, t, 0.0);

    // Surface buoy attitude
    const Eigen::Matrix3d C_wb = rotationMatrixAt(x, y, t);

    const Eigen::Vector3d g_world(0.0, 0.0, -g_);
    imu.accel_body  = C_wb * (state.acceleration - g_world);
    imu.accel_debug = C_wb * (-g_world);

    const Eigen::Matrix3d C_prev = rotationMatrixAt(x, y, t - 0.5 * dt);
    const Eigen::Matrix3d C_next = rotationMatrixAt(x, y, t + 0.5 * dt);
    const Eigen::Matrix3d Cdot   = (C_next - C_prev) / dt;

    Eigen::Matrix3d Omega = -Cdot * C_wb.transpose();
    Omega = 0.5 * (Omega - Omega.transpose());

    imu.gyro_body.x() = Omega(2,1);
    imu.gyro_body.y() = Omega(0,2);
    imu.gyro_body.z() = Omega(1,0);

    return imu;
}

    // Directional Spectrum API
    // Compute directional spectrum at a given frequency f and angle θ
    double directionalSpectrumValue(double f, double theta) const {
      auto &freqs = spectrum_.frequencies();
      int idx = int(std::lower_bound(freqs.data(), freqs.data() + N_FREQ, f) - freqs.data());
      if (idx < 0 || idx >= N_FREQ) return 0.0;

      const double S_f = spectrum_.spectrum()(idx);
      return S_f * (*directional_dist_)(theta, f);
    }

    // Discrete directional spectrum, size N_FREQ × M
    // If normalize = true, weights are normalized so that ∑ D(θ; f) Δθ ≈ 1.
    Eigen::MatrixXd getDirectionalSpectrum(int M, bool normalize = true) const {
      Eigen::MatrixXd E(N_FREQ, M);
      for (int i = 0; i < N_FREQ; ++i) {
        double f = spectrum_.frequencies()(i);
        std::vector<double> weights = normalize
                                      ? directional_dist_->normalized_weights(M, f)
                                      : directional_dist_->weights(M, f);
        double S_f = spectrum_.spectrum()(i);
        for (int m = 0; m < M; ++m) {
          E(i, m) = S_f * weights[m];
        }
      }
      return E;
    }

    const Eigen::Matrix<double, N_FREQ, 1>& spectrum() const {
      return spectrum_.spectrum();
    }
    const Eigen::Matrix<double, N_FREQ, 1>& frequencies() const {
      return spectrum_.frequencies();
    }
    const Eigen::Matrix<double, N_FREQ, 1>& amplitudes() const {
      return spectrum_.amplitudes();
    }
    const Eigen::Matrix<double, N_FREQ, 1>& df() const {
      return spectrum_.df();
    }

    double gamma() const {
      return spectrum_.gamma();
    }

  private:
    JonswapSpectrum<N_FREQ> spectrum_;
    std::shared_ptr<DirectionalDistribution> directional_dist_;
    double g_;
    DeepWaterSecondOrder model_;

    static std::vector<DeepWaterSecondOrder::Component> makeComponents(
        const JonswapSpectrum<N_FREQ>& spectrum,
        const std::shared_ptr<DirectionalDistribution>& distribution,
        unsigned int seed, double gravity) {
      if (!distribution)
        throw std::invalid_argument("DirectionalDistribution must not be null");
      const auto& frequencies=spectrum.frequencies();
      const auto directions=distribution->sample_directions_for_frequencies(
          std::vector<double>(frequencies.data(),frequencies.data()+N_FREQ));
      if (directions.size()!=N_FREQ)
        throw std::invalid_argument("DirectionalDistribution returned wrong number of directions");
      std::mt19937 rng(seed);
      std::uniform_real_distribution<double> uniform(0,2*PI);
      std::vector<DeepWaterSecondOrder::Component> components;
      components.reserve(N_FREQ);
      for (int i=0; i<N_FREQ; ++i) {
        const double w=2*PI*frequencies(i);
        if (!std::isfinite(w*w/gravity))
          throw std::invalid_argument("Nonfinite wave number");
        // Independent phases, preserving the public eta_1 = a sin(theta)
        // convention via the cosine kernel's phase shift.
        components.push_back({spectrum.amplitudes()(i),w,directions[i],uniform(rng)-PI/2});
      }
      return components;
    }

    Eigen::Matrix3d rotationMatrixAt(double x, double y, double t) const {
      return orientationFromSlopes(getLagrangianSurfaceSlopes(x,y,t));
    }

};

#ifdef JONSWAP_TEST
// CSV generator for testing
static void generateWaveJonswapCSV(const std::string& filename,
                                   double Hs, double Tp, double mean_dir_deg,
                                   double duration = 40.0, double dt = 0.005) {
  constexpr int N = 128;
  auto dist = std::make_shared<Cosine2sRandomizedDistribution>(
                  mean_dir_deg * PI / 180.0, 10.0, 42u);
  auto waveModel = std::make_unique<Jonswap3dStokesWaves<N>>(
                      Hs, Tp, dist, 0.02, 0.8, 3.3, g_std, 42u);

  const int N_time = static_cast<int>(duration / dt) + 1;
  Eigen::ArrayXd time = Eigen::ArrayXd::LinSpaced(N_time, 0.0, duration);

  Eigen::ArrayXXd disp(3, N_time), vel(3, N_time), acc(3, N_time);
  Eigen::ArrayXXd accel_body(3, N_time), gyro_body(3, N_time);
  Eigen::ArrayXXd euler_deg(3, N_time); // roll, pitch, yaw

  for (int i = 0; i < N_time; ++i) {
    double t = time(i);

    auto state = waveModel->getLagrangianState(0.0, 0.0, t, 0.0);

    // use SAME dt as CSV output
    auto imu = waveModel->getIMUReadings(0.0, 0.0, t, 0.0, dt);

    for (int j = 0; j < 3; ++j) {
      disp(j, i) = state.displacement(j);
      vel(j, i)  = state.velocity(j);
      acc(j, i)  = state.acceleration(j);
      accel_body(j, i) = imu.accel_body(j);
      gyro_body(j, i)  = imu.gyro_body(j);
    }

    // Reference Euler from full orientation
    Eigen::Vector3d euler = waveModel->getEulerAngles(0.0, 0.0, t);

    euler_deg(0, i) = euler.x();
    euler_deg(1, i) = euler.y();
    euler_deg(2, i) = euler.z();
  }

  std::ofstream file(filename);
  file << "time,disp_x,disp_y,disp_z,vel_x,vel_y,vel_z,acc_x,acc_y,acc_z,"
       << "accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,roll_deg,pitch_deg,yaw_deg\n";

  for (int i = 0; i < N_time; ++i) {
    file << time(i) << ","
         << disp(0, i) << "," << disp(1, i) << "," << disp(2, i) << ","
         << vel(0, i)  << "," << vel(1, i)  << "," << vel(2, i)  << ","
         << acc(0, i)  << "," << acc(1, i)  << "," << acc(2, i)  << ","
         << accel_body(0, i) << "," << accel_body(1, i) << "," << accel_body(2, i) << ","
         << gyro_body(0, i)  << "," << gyro_body(1, i)  << "," << gyro_body(2, i) << ","
         << euler_deg(0, i) << "," << euler_deg(1, i) << "," << euler_deg(2, i) << "\n";
  }
}

static void exportDirectionalSpectrumCSV(const std::string& filename,
    double Hs, double Tp,
    double mean_dir_deg = 0.0,
    int N_freq = 128, int N_theta = 72) {
  auto dist = std::make_shared<Cosine2sRandomizedDistribution>(mean_dir_deg * PI / 180.0, 10.0, 42u);
  auto waveModel = std::make_unique<Jonswap3dStokesWaves<128>>(Hs, Tp, dist, 0.02, 0.8, 3.3, g_std, 42u);
  auto freqs = waveModel->frequencies();
  Eigen::MatrixXd E = waveModel->getDirectionalSpectrum(N_theta);

  std::ofstream file(filename);
  file << "f_Hz,theta_deg,E\n";

  const double dtheta = 360.0 / N_theta;
  for (int i = 0; i < N_freq; ++i) {
    for (int m = 0; m < N_theta; ++m) {
      double theta_deg = -180.0 + m * dtheta;
      file << freqs(i) << "," << theta_deg << "," << E(i, m) << "\n";
    }
  }
}

static void Jonswap_testWavePatterns() {
  generateWaveJonswapCSV("short_waves_stokes.csv",  0.5,  3.0, 30.0);
  generateWaveJonswapCSV("medium_waves_stokes.csv", 2.0,  7.0, 30.0);
  generateWaveJonswapCSV("long_waves_stokes.csv",   4.0, 12.0, 30.0);
}

static void Jonswap_testWaveSpectrum() {
  exportDirectionalSpectrumCSV("short_waves_jonswap_spectrum.csv",  0.5,  3.0, 30.0, 128, 72);
  exportDirectionalSpectrumCSV("medium_waves_jonswap_spectrum.csv", 2.0,  7.0, 30.0, 128, 72);
  exportDirectionalSpectrumCSV("long_waves_jonswap_spectrum.csv",   4.0, 12.0, 30.0, 128, 72);
}
#endif
