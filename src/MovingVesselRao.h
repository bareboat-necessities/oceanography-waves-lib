#pragma once

#include "VesselRao.h"

// Prescribed slow steering plus the existing estimated wave-response surrogate.
// World +x is north, +y west, +z up; positive heading turns toward port/west.
// This is a quasi-steady encounter-frequency extension, not a hull-specific
// forward-speed hydrodynamic or rudder/propulsion model. See doc/vessel-rao.md.
class MovingVesselRao {
public:
    struct Motion {
        double speed = 5.0*1852.0/3600.0; // m/s; constant speed along heading
        double heading_amplitude = 3.0*std::acos(-1.0)/180.0;
        double heading_period = 120.0; // s
    };
    struct Track {
        Eigen::Vector3d displacement = Eigen::Vector3d::Zero();
        Eigen::Vector3d velocity = Eigen::Vector3d::Zero();
        Eigen::Vector3d acceleration = Eigen::Vector3d::Zero();
        double heading, heading_rate, heading_acceleration;
    };

    MovingVesselRao(const std::vector<WaveHarmonic>& waves, const VesselRao::Parameters& p)
        : MovingVesselRao(waves,p,Motion{}) {}

    MovingVesselRao(const std::vector<WaveHarmonic>& waves, const VesselRao::Parameters& p,
                   const Motion& motion) : rao_(waves,p), waves_(waves), motion_(motion) {
        if (!(std::isfinite(motion.speed) && motion.speed>=0 &&
              std::isfinite(motion.heading_amplitude) && motion.heading_amplitude>=0 &&
              motion.heading_amplitude<=std::acos(-1.0)/6 &&
              std::isfinite(motion.heading_period) && motion.heading_period>0))
            throw std::invalid_argument("MovingVesselRao: invalid speed or slow steering parameters");
        frequency_=2*std::acos(-1.0)/motion.heading_period;
        // Fourier integral of exp(i A sin(Omega t)). At A <= pi/6,
        // omitted terms n>=13 are below 6e-18. No time-stepping drift.
        for (int n=0;n<=12;++n) {
            double term=1;
            for (int j=1;j<=n;++j) term*=motion.heading_amplitude/(2*j);
            bessel_[n]=term;
            for (int j=1;j<=16;++j) {
                term*=-motion.heading_amplitude*motion.heading_amplitude/(4*j*(j+n));
                bessel_[n]+=term;
            }
        }
    }

    Track track(double t) const {
        if (!std::isfinite(t)) throw std::invalid_argument("MovingVesselRao: invalid time");
        Track out;
        const double phase=frequency_*t, a=motion_.heading_amplitude, u=motion_.speed;
        out.heading=rao_.parameters().heading+a*std::sin(phase);
        out.heading_rate=a*frequency_*std::cos(phase);
        out.heading_acceleration=-a*frequency_*frequency_*std::sin(phase);
        double x=bessel_[0]*t, y=0;
        for (int n=1;n<=12;++n) {
            if (n%2==0) x+=2*bessel_[n]*std::sin(n*phase)/(n*frequency_);
            else y+=2*bessel_[n]*(1-std::cos(n*phase))/(n*frequency_);
        }
        const double mean=rao_.parameters().heading;
        out.displacement=u*Eigen::Vector3d(std::cos(mean)*x-std::sin(mean)*y,
                                          std::sin(mean)*x+std::cos(mean)*y,0);
        out.velocity=u*Eigen::Vector3d(std::cos(out.heading),std::sin(out.heading),0);
        out.acceleration=u*out.heading_rate*
            Eigen::Vector3d(-std::sin(out.heading),std::cos(out.heading),0);
        return out;
    }

    VesselRao::State state(double t) const {
        if (motion_.speed==0 && motion_.heading_amplitude==0) return rao_.state(t);
        const auto path=track(t);
        using Vector6 = Eigen::Matrix<double,6,1>;
        Vector6 pose=Vector6::Zero(), rate=pose, acceleration=pose;
        const std::complex<double> i(0,1);
        constexpr double dpsi=1e-4; // rad; differentiate only the slowly varying RAO
        for (const auto& w : waves_) {
            const Eigen::Vector3d k(w.wavenumber*std::cos(w.direction),
                                     w.wavenumber*std::sin(w.direction),0);
            const double encounter=w.omega-k.dot(path.velocity);
            const auto phase=std::polar(w.amplitude,k.dot(path.displacement)-w.omega*t+w.phase);
            const auto h=response(w,path.heading);
            const auto lo=response(w,path.heading-dpsi), hi=response(w,path.heading+dpsi);
            for (int j=0;j<6;++j) {
                const auto derivative=(hi[j]-lo[j])/(2*dpsi);
                const auto hdot=derivative*path.heading_rate;
                const auto hddot=(hi[j]-2.0*h[j]+lo[j])/(dpsi*dpsi)*
                    (path.heading_rate*path.heading_rate)+derivative*path.heading_acceleration;
                pose(j)+=(h[j]*phase).real();
                rate(j)+=((hdot-i*encounter*h[j])*phase).real();
                acceleration(j)+=((hddot-2.0*i*encounter*hdot+
                    (i*k.dot(path.acceleration)-encounter*encounter)*h[j])*phase).real();
            }
        }
        VesselRao::State out;
        out.displacement=path.displacement+pose.head<3>();
        out.velocity=path.velocity+rate.head<3>();
        out.acceleration=path.acceleration+acceleration.head<3>();
        out.euler=Eigen::Vector3d(pose(3),pose(4),path.heading);
        const double roll=pose(3), pitch=pose(4), yaw_rate=path.heading_rate;
        const Eigen::Matrix3d body_to_world=(
            Eigen::AngleAxisd(path.heading,Eigen::Vector3d::UnitZ())*
            Eigen::AngleAxisd(pitch,Eigen::Vector3d::UnitY())*
            Eigen::AngleAxisd(roll,Eigen::Vector3d::UnitX())).toRotationMatrix();
        out.world_to_body=body_to_world.transpose();
        out.accel_body=out.world_to_body*(out.acceleration+
            Eigen::Vector3d(0,0,rao_.parameters().gravity));
        out.gyro_body=Eigen::Vector3d(rate(3)-yaw_rate*std::sin(pitch),
            rate(4)*std::cos(roll)+yaw_rate*std::sin(roll)*std::cos(pitch),
            -rate(4)*std::sin(roll)+yaw_rate*std::cos(roll)*std::cos(pitch));
        return out;
    }

private:
    VesselRao rao_;
    std::vector<WaveHarmonic> waves_;
    Motion motion_;
    double frequency_;
    std::array<double,13> bessel_{};

    VesselRao::Transfer response(const WaveHarmonic& wave, double heading) const {
        const double encounter=wave.omega-wave.wavenumber*motion_.speed*
            std::cos(wave.direction-heading);
        auto h=rao_.encounterTransfer(encounter,wave.wavenumber,wave.direction,heading);
        // Translate the response in instantaneous vessel axes into world axes
        // BEFORE differentiating; this retains the rotating-frame terms.
        const auto surge=h[0], sway=h[1];
        h[0]=std::cos(heading)*surge-std::sin(heading)*sway;
        h[1]=std::sin(heading)*surge+std::cos(heading)*sway;
        return h;
    }
};
