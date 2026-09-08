// Full-record diagnostic, not a physical-validity certificate or a g-clamp gate.
// Usage: reference_acceleration_audit [duration_seconds=1200]
// Build the same source against the pre-fix headers with -DOWLIB_LEGACY_PHASES
// to reproduce the original JONSWAP records. PM is unchanged in this PR.
#define EIGEN_NON_ARDUINO
#include "Jonswap3dStokesWaves.h"
#include "PiersonMoskowitzStokes3D_Waves.h"
#include <iomanip>
#include <iostream>

struct Audit {
    double maximum=0,time=0,vertical=0,linear=0,quadratic=0,sum_linear=0,sum_quadratic=0;
    size_t above_g=0;
    void add(double t,const Eigen::Vector3d& a,double linear_z) {
        const double norm=a.norm();
        if (norm>maximum) { maximum=norm; time=t; }
        above_g += norm>9.80665;
        vertical=std::max(vertical,std::abs(a.z()));
        linear=std::max(linear,std::abs(linear_z));
        quadratic=std::max(quadratic,std::abs(a.z()-linear_z));
        sum_linear+=linear_z*linear_z;
        sum_quadratic+=(a.z()-linear_z)*(a.z()-linear_z);
    }
    void print(const char* model,double hs,double tp,size_t count) const {
        std::cout << model << ',' << hs << ',' << tp << ',' << count << ','
                  << maximum << ',' << time << ',' << above_g << ',' << vertical << ',';
        if (std::string(model).find("jonswap")==0)
            std::cout << linear << ',' << quadratic << ',' << std::sqrt(sum_linear/count)
                      << ',' << std::sqrt(sum_quadratic/count);
        else std::cout << ",,,";
        std::cout << std::endl;
    }
};

int main(int argc,char** argv) {
    const double duration=argc==2 ? std::stod(argv[1]) : 1200;
    if (!(std::isfinite(duration)&&duration>0&&duration<=1200)) return 2;
    const size_t count=static_cast<size_t>(std::llround(duration*200));
    if (count==0) return 2;
    constexpr double dt=double(1.0f/200.0f);
    const double heights[]={double(0.27f),1.5,4.0,8.5};
    const double periods[]={3.0,double(5.7f),8.5,double(11.4f)};
    std::cout << std::setprecision(12)
              << "model,Hs_m,Tp_s,samples,max_accel_mps2,max_time_s,above_g_samples,"
                 "max_abs_az_mps2,max_abs_linear_az_mps2,max_abs_second_az_mps2,"
                 "rms_linear_az_mps2,rms_second_az_mps2\n";
    for (int scenario=0;scenario<4;++scenario) {
        const double direction=(scenario==0||scenario==2 ? 30.0 : -30.0)*M_PI/180;
        auto dist=std::make_shared<Cosine2sRandomizedDistribution>(direction,10.0,42u);
        auto wave=std::make_unique<Jonswap3dStokesWaves<128>>(
            heights[scenario],periods[scenario],dist,0.02,0.8,3.3,9.80665,42u);
        std::mt19937 rng(42u);
        std::uniform_real_distribution<double> uniform(0,2*M_PI);
        Eigen::Array<double,128,1> phase;
#if defined(OWLIB_LEGACY_PHASES) || defined(OWLIB_REPLAY_LEGACY_PHASES)
        for(int i=0;i<64;++i) { phase(i)=uniform(rng); phase(127-i)=-phase(i); }
#else
        for(int i=0;i<128;++i) phase(i)=uniform(rng);
#endif
        const Eigen::Array<double,128,1> omega=2*M_PI*wave->frequencies().array();
        const Eigen::Array<double,128,1> aomega2=wave->amplitudes().array()*omega.square();
#ifdef OWLIB_REPLAY_LEGACY_PHASES
        Cosine2sRandomizedDistribution replay_dist(direction,10.0,42u);
        const auto frequencies=wave->frequencies();
        const auto directions=replay_dist.sample_directions_for_frequencies(
            std::vector<double>(frequencies.data(),frequencies.data()+128));
        std::vector<DeepWaterSecondOrder::Component> components;
        for(int i=0;i<128;++i)
            components.push_back({wave->amplitudes()(i),omega(i),directions[i],phase(i)-M_PI/2});
        DeepWaterSecondOrder replay(components,9.80665);
#endif
        Audit audit;
        for(size_t i=0;i<count;++i) {
            const double t=i*dt;
#ifdef OWLIB_REPLAY_LEGACY_PHASES
            const auto state=replay.state(0,0,t,0);
#else
            const auto state=wave->getLagrangianState(0,0,t,0);
#endif
            const double linear=-(aomega2*(phase-omega*t).sin()).sum();
            audit.add(t,state.acceleration,linear);
        }
#ifdef OWLIB_REPLAY_LEGACY_PHASES
        audit.print("jonswap_legacy_phases",heights[scenario],periods[scenario],count);
#else
        audit.print("jonswap",heights[scenario],periods[scenario],count);
#endif
        auto pm_dist=std::make_shared<Cosine2sRandomizedDistribution>(direction,10.0,42u);
        auto pm=std::make_unique<PMStokesN3dWaves<128,3>>(
            heights[scenario],periods[scenario],pm_dist,0.02,0.8,9.80665,42u);
        Audit pm_audit;
        for(size_t i=0;i<count;++i) {
            const double t=i*dt;
            pm_audit.add(t,pm->getLagrangianState(t).acceleration,0);
        }
        pm_audit.print("pmstokes",heights[scenario],periods[scenario],count);
    }
}
