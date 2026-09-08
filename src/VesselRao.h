#pragma once

#ifdef EIGEN_NON_ARDUINO
#include <Eigen/Geometry>
#else
#include <ArduinoEigenDense.h>
#endif
#include <array>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>
#include "WaveHarmonic.h"

// Estimated, zero-speed displacement RAOs for a fin-keel sailboat family.
// This is a configurable response surrogate, not measured hull data.
// Axes: x forward, y port, z up. IMU and response origin are at the CG.
// See doc/vessel-rao.md for equations, units, assumptions and limitations.
class VesselRao {
public:
    struct Parameters {
        double waterline_length = 7.0; // m; nominal LOA 8.53 m
        double beam = 2.9;             // m
        double draft = 1.5;            // m, including fin keel
        double heave_period = 2.4;     // s, estimated natural periods
        double pitch_period = 2.8;
        double roll_period = 3.5;
        double heave_damping = 0.45;   // fractions of critical damping
        double pitch_damping = 0.35;
        double roll_damping = 0.22;
        double surge_time = 0.7;       // s, nonresonant horizontal response
        double sway_time = 1.0;
        double heading = 0.0;          // rad, fixed mean heading in world frame
        double gravity = 9.80665;
    };
    // Geometrically similar hull family: lengths scale by R, times by sqrt(R).
    // The 28 ft preset returns the original defaults exactly.
    static Parameters sailboat(int length_feet) {
        if (length_feet != 28 && length_feet != 34 && length_feet != 42 && length_feet != 50)
            throw std::invalid_argument("VesselRao: supported sailboats are 28, 34, 42, 50 ft");
        Parameters p;
        const double ratio=length_feet/28.0, time_scale=std::sqrt(ratio);
        p.waterline_length *= ratio;
        p.beam *= ratio;
        p.draft *= ratio;
        p.heave_period *= time_scale;
        p.pitch_period *= time_scale;
        p.roll_period *= time_scale;
        p.surge_time *= time_scale;
        p.sway_time *= time_scale;
        return p;
    }

    using Transfer = std::array<std::complex<double>, 6>; // surge,sway,heave,roll,pitch,yaw
    struct State {
        Eigen::Vector3d displacement = Eigen::Vector3d::Zero(); // world, m
        Eigen::Vector3d velocity = Eigen::Vector3d::Zero();     // world, m/s
        Eigen::Vector3d acceleration = Eigen::Vector3d::Zero();// world, m/s^2, excludes g
        Eigen::Vector3d euler = Eigen::Vector3d::Zero();        // roll,pitch,yaw, rad
        Eigen::Matrix3d world_to_body = Eigen::Matrix3d::Identity();
        Eigen::Vector3d accel_body = Eigen::Vector3d::Zero();   // specific force, includes gravity
        Eigen::Vector3d gyro_body = Eigen::Vector3d::Zero();    // rad/s
    };

    explicit VesselRao(const std::vector<WaveHarmonic>& waves)
        : VesselRao(waves, Parameters{}) {}

    VesselRao(const std::vector<WaveHarmonic>& waves, const Parameters& parameters)
        : p_(parameters) {
        for (double v : {p_.waterline_length,p_.beam,p_.draft,p_.heave_period,
                         p_.pitch_period,p_.roll_period,p_.heave_damping,
                         p_.pitch_damping,p_.roll_damping,p_.surge_time,
                         p_.sway_time,p_.gravity})
            if (!(std::isfinite(v) && v > 0))
                throw std::invalid_argument("VesselRao: parameters must be finite and positive");
        if (!std::isfinite(p_.heading)) throw std::invalid_argument("VesselRao: invalid heading");
        heading_rotation_ = Eigen::AngleAxisd(p_.heading, Eigen::Vector3d::UnitZ()).toRotationMatrix();
        for (const auto& w : waves) {
            if (!(std::isfinite(w.amplitude) && w.amplitude >= 0 && std::isfinite(w.phase)))
                throw std::invalid_argument("VesselRao: invalid wave amplitude or phase");
            auto h = transfer(w.omega, w.wavenumber, w.direction);
            const auto elevation = std::polar(w.amplitude, w.phase);
            for (auto& v : h) v *= elevation;
            modes_.push_back({w.omega, h});
        }
    }

    // Translation: m/m. Rotation: rad/m. Convention Re[H a exp(i phase-i omega t)].
    Transfer transfer(double omega, double k, double direction) const {
        if (!(std::isfinite(omega) && omega >= 0 && std::isfinite(k) && k >= 0 &&
              std::isfinite(direction)))
            throw std::invalid_argument("VesselRao: invalid frequency, wave number or direction");
        const std::complex<double> i(0,1);
        const double beta=direction-p_.heading, c=std::cos(beta), s=std::sin(beta);
        // Smooth spatial averaging with the second moment of a rectangular waterplane.
        const double qx=k*p_.waterline_length*c, qy=k*p_.beam*s;
        const double footprint=std::exp(-(qx*qx+qy*qy)/24.0);
        const double horizontal=footprint*std::exp(-0.35*k*p_.draft);
        auto lowpass = [&](double tau) { const auto d=1.0-i*omega*tau; return 1.0/(d*d); };
        return {i*c*horizontal*lowpass(p_.surge_time),
                i*s*horizontal*lowpass(p_.sway_time),
                footprint*oscillator(omega,p_.heave_period,p_.heave_damping),
                i*k*s*footprint*oscillator(omega,p_.roll_period,p_.roll_damping),
                -i*k*c*footprint*oscillator(omega,p_.pitch_period,p_.pitch_damping),
                0.0}; // heading held fixed: no wave yaw, sailing trim or forward speed
    }

    State state(double t) const {
        if (!std::isfinite(t)) throw std::invalid_argument("VesselRao: invalid time");
        Eigen::Matrix<double,6,1> pose=Eigen::Matrix<double,6,1>::Zero();
        Eigen::Matrix<double,6,1> rate=pose, acceleration=pose;
        for (const auto& m : modes_) {
            const auto phase=std::polar(1.0,-m.omega*t);
            for (int j=0; j<6; ++j) {
                const auto v=m.response[j]*phase;
                pose(j)+=v.real();
                rate(j)+=m.omega*v.imag();
                acceleration(j)-=m.omega*m.omega*v.real();
            }
        }
        State out;
        out.displacement=heading_rotation_*pose.head<3>();
        out.velocity=heading_rotation_*rate.head<3>();
        out.acceleration=heading_rotation_*acceleration.head<3>();
        out.euler=Eigen::Vector3d(pose(3),pose(4),p_.heading);
        const double roll=pose(3), pitch=pose(4);
        const Eigen::Matrix3d body_to_world=heading_rotation_*
            Eigen::AngleAxisd(pitch,Eigen::Vector3d::UnitY()).toRotationMatrix()*
            Eigen::AngleAxisd(roll,Eigen::Vector3d::UnitX()).toRotationMatrix();
        out.world_to_body=body_to_world.transpose();
        out.accel_body=out.world_to_body*(out.acceleration+Eigen::Vector3d(0,0,p_.gravity));
        // Exact body rate for Rz(heading) Ry(pitch) Rx(roll), with fixed heading.
        out.gyro_body=Eigen::Vector3d(rate(3),rate(4)*std::cos(roll),-rate(4)*std::sin(roll));
        return out;
    }

    const Parameters& parameters() const { return p_; }

private:
    struct Mode { double omega; Transfer response; };
    Parameters p_;
    Eigen::Matrix3d heading_rotation_;
    std::vector<Mode> modes_;
    static std::complex<double> oscillator(double omega, double period, double damping) {
        const double ratio=omega*period/(2*std::acos(-1.0));
        return 1.0/std::complex<double>(1-ratio*ratio,-2*damping*ratio);
    }
};
