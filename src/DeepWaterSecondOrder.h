#pragma once

#ifdef EIGEN_NON_ARDUINO
#include <Eigen/Dense>
#else
#include <ArduinoEigenDense.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

// Second-order potential-flow solution and its Lagrangian particle map.
// See doc/jonswap-second-order.md for the derivation, phase convention, and
// limits of this weakly nonlinear, deep-water, nonbreaking model.
class DeepWaterSecondOrder {
public:
    struct Component {
        double amplitude, omega, direction, phase; // eta_1 = a cos(k.x-wt+phase)
    };
    struct State {
        Eigen::Vector3d displacement = Eigen::Vector3d::Zero();
        Eigen::Vector3d velocity = Eigen::Vector3d::Zero();
        Eigen::Vector3d acceleration = Eigen::Vector3d::Zero();
    };

    explicit DeepWaterSecondOrder(const std::vector<Component>& components,
                                  double gravity = 9.80665) : g_(gravity) {
        if (!(std::isfinite(g_) && g_ > 0))
            throw std::invalid_argument("DeepWaterSecondOrder: gravity must be finite and positive");
        for (const auto& c : components) {
            if (!(std::isfinite(c.amplitude) && c.amplitude >= 0 &&
                  std::isfinite(c.omega) && c.omega > 0 &&
                  std::isfinite(c.direction) && std::isfinite(c.phase)))
                throw std::invalid_argument("DeepWaterSecondOrder: invalid component");
            if (c.amplitude == 0) continue;
            Mode m{c.amplitude, c.omega, c.omega*c.omega/g_,
                   Eigen::Vector2d(std::cos(c.direction), std::sin(c.direction)), c.phase};
            if (!(std::isfinite(m.k) && m.k > 0))
                throw std::invalid_argument("DeepWaterSecondOrder: unrepresentable wave number");
            modes_.push_back(m);
        }
        std::vector<Pair> pairs;
        for (size_t i = 0; i < modes_.size(); ++i)
            for (size_t j = i; j < modes_.size(); ++j)
                for (int sign : {1, -1}) {
                    pairs.push_back(makePair(modes_[i], modes_[j], sign, i == j ? 0.5 : 1.0));
                    pair_i_.push_back(i); pair_j_.push_back(j); pair_sign_.push_back(sign);
                }
        const auto n = static_cast<Eigen::Index>(pairs.size());
        omega_.resize(n); k_.resize(n); ksum_.resize(n); kx_.resize(n); ky_.resize(n);
        eta_.resize(n); potential_.resize(n);
        hx_p_.resize(n); hy_p_.resize(n); hx_a_.resize(n); hy_a_.resize(n);
        z_p_.resize(n); z_a_.resize(n);
        for (Eigen::Index i = 0; i < n; ++i) {
            const auto& p = pairs[static_cast<size_t>(i)];
            omega_(i)=p.omega; k_(i)=p.k; ksum_(i)=p.ksum;
            kx_(i)=p.q.x(); ky_(i)=p.q.y();
            eta_(i)=p.eta; potential_(i)=p.potential;
            hx_p_(i)=p.hp.x(); hy_p_(i)=p.hp.y();
            hx_a_(i)=p.ha.x(); hy_a_(i)=p.ha.y();
            z_p_(i)=p.zp; z_a_(i)=p.za;
        }
        hx_surface_=hx_p_+hx_a_; hy_surface_=hy_p_+hy_a_; z_surface_=z_p_+z_a_;
    }

    // x,y,z are particle labels, not instantaneous Eulerian coordinates.
    // Horizontal second-order displacement is integrated from t=0, including
    // steady drift. z=0 labels the surface; z<0 labels subsurface particles.
    State state(double x, double y, double t, double z = 0) const {
        checkCoordinates(x,y,t,z);
        if (z > 0) throw std::invalid_argument("Particle label z must be <= 0");
        State out;
        for (const auto& m : modes_) {
            const double arg=m.k*m.direction.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase;
            const double a=m.a*std::exp(m.k*z), s=std::sin(arg), c=std::cos(arg);
            out.displacement.head<2>() -= a*s*m.direction;
            out.displacement.z() += a*c;
            out.velocity.head<2>() += a*m.omega*c*m.direction;
            out.velocity.z() += a*m.omega*s;
            out.acceleration.head<2>() += a*m.omega*m.omega*s*m.direction;
            out.acceleration.z() -= a*m.omega*m.omega*c;
        }
        updateTrig(x,y,t);
        if (z == 0) {
            accumulatePairs(out,hx_surface_,hy_surface_,z_surface_);
        } else {
            const Eigen::ArrayXd ep=(k_*z).exp(), ea=(ksum_*z).exp();
            accumulatePairs(out,hx_p_*ep+hx_a_*ea,hy_p_*ep+hy_a_*ea,z_p_*ep+z_a_*ea);
        }
        return out;
    }

    double surfaceElevation(double x, double y, double t) const {
        checkCoordinates(x,y,t,0);
        double eta=0;
        for (const auto& m : modes_)
            eta += m.a*std::cos(m.k*m.direction.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase);
        updateTrig(x,y,t);
        return eta+(eta_*cos_).sum();
    }

    // Eulerian potential, including its time-dependent spatially uniform term.
    // Evaluation above z=0 is an analytic continuation used by perturbation
    // checks, not a claim of validity far above still water.
    double velocityPotential(double x, double y, double z, double t) const {
        checkCoordinates(x,y,t,z);
        double phi=0;
        for (const auto& m : modes_)
            phi += m.a*g_/m.omega*std::exp(m.k*z)*
                std::sin(m.k*m.direction.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase);
        updateTrig(x,y,t);
        return phi+(potential_*(k_*z).exp()*sin_).sum();
    }

    Eigen::Vector2d surfaceSlopes(double x, double y, double t) const {
        checkCoordinates(x,y,t,0);
        Eigen::Vector2d slope=Eigen::Vector2d::Zero();
        for (const auto& m : modes_)
            slope -= m.a*m.k*m.direction*
                std::sin(m.k*m.direction.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase);
        updateTrig(x,y,t);
        slope.x() -= (eta_*kx_*sin_).sum();
        slope.y() -= (eta_*ky_*sin_).sum();
        return slope;
    }

    // Surface gradient pulled back to a particle label, through second order:
    // grad eta_1 + grad eta_2 + Hessian(eta_1) * xi_1,h.
    Eigen::Vector2d particleSurfaceSlopes(double x, double y, double t) const {
        Eigen::Vector2d slope=surfaceSlopes(x,y,t), xi=Eigen::Vector2d::Zero();
        Eigen::Matrix2d hessian=Eigen::Matrix2d::Zero();
        for (const auto& m : modes_) {
            const double arg=m.k*m.direction.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase;
            xi -= m.a*std::sin(arg)*m.direction;
            hessian -= m.a*m.k*m.k*std::cos(arg)*(m.direction*m.direction.transpose());
        }
        return slope+hessian*xi;
    }

private:
    struct Mode { double a, omega, k; Eigen::Vector2d direction; double phase; };
    struct Pair {
        double omega, k, ksum, eta=0, potential=0, zp=0, za=0;
        Eigen::Vector2d q, hp=Eigen::Vector2d::Zero(), ha=Eigen::Vector2d::Zero();
    };
    double g_;
    std::vector<Mode> modes_;
    Eigen::ArrayXd omega_, k_, ksum_, kx_, ky_, eta_, potential_;
    Eigen::ArrayXd hx_p_, hy_p_, hx_a_, hy_a_, z_p_, z_a_;
    Eigen::ArrayXd hx_surface_, hy_surface_, z_surface_;
    std::vector<size_t> pair_i_,pair_j_;
    std::vector<int> pair_sign_;
    mutable double cache_x_=std::numeric_limits<double>::quiet_NaN(), cache_y_=0, cache_t_=0;
    mutable Eigen::ArrayXd sin_, cos_, integral_cos_, sin0_, cos0_;
    mutable std::vector<double> mode_sin_,mode_cos_;

    static void checkCoordinates(double x,double y,double t,double z) {
        if (!(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(t)&&std::isfinite(z)))
            throw std::invalid_argument("Wave coordinates and time must be finite");
    }

    Pair makePair(const Mode& i, const Mode& j, int sign, double multiplicity) const {
        const double a=multiplicity*i.a*j.a, wi=i.omega, wj=j.omega;
        Pair p;
        p.omega=wi+sign*wj; p.ksum=i.k+j.k;
        const double signedKDifference=(wi-wj)*(wi+wj)/g_;
        p.q=(sign == 1 ? i.k+j.k : signedKDifference)*i.direction +
            sign*j.k*(j.direction-i.direction);
        const double oneMinusCos=0.5*(i.direction-j.direction).squaredNorm();
        const double onePlusCos=0.5*(i.direction+j.direction).squaredNorm();
        p.k=std::hypot(signedKDifference,
            std::sqrt(2*i.k*j.k*(sign == 1 ? onePlusCos : oneMinusCos)));
        if (sign == -1 && wi == wj && oneMinusCos == 0) {
            // Exact zero-frequency diagonal (also two identical wavevectors).
            p.ha=2*a*i.k*wi*i.direction;
            p.za=a*i.k;
            return p;
        }
        const double coupling=sign == 1 ? -oneMinusCos : onePlusCos;
        const double adv=sign == 1 ? oneMinusCos : onePlusCos;
        // Algebraically equal to Omega^2-g|q|, avoiding subtractive cancellation
        // for nearly coincident difference-frequency components.
        const double kd=std::abs(p.omega)*(wi+wj)/g_;
        const double denom=sign == 1 ?
            2*wi*wj + g_*2*i.k*j.k*oneMinusCos/(p.ksum+p.k) :
            -2*std::min(wi,wj)*std::abs(p.omega) -
                g_*2*i.k*j.k*oneMinusCos/(p.k+kd);
        if (!std::isfinite(denom) || denom == 0)
            throw std::runtime_error("Unresolved second-order interaction denominator");
        p.potential=a*p.omega*coupling*wi*wj/denom;
        p.eta=(0.5*a*(wi*wi+wj*wj-coupling*wi*wj)+p.omega*p.potential)/g_;
        p.hp=p.potential*p.q;
        p.ha=0.5*a*adv*(i.k*wi*i.direction+j.k*wj*j.direction);
        p.zp=a*coupling*wi*wj*(p.k/denom);
        p.za=0.5*a*adv*(wi*wi-sign*wi*wj+wj*wj)/g_;
        return p;
    }

    void accumulatePairs(State& out,const Eigen::ArrayXd& hx,
                         const Eigen::ArrayXd& hy,const Eigen::ArrayXd& vz) const {
        out.displacement.x() += (hx*integral_cos_).sum();
        out.displacement.y() += (hy*integral_cos_).sum();
        out.displacement.z() += (vz*cos_).sum();
        out.velocity.x() += (hx*cos_).sum();
        out.velocity.y() += (hy*cos_).sum();
        out.velocity.z() += (vz*omega_*sin_).sum();
        out.acceleration.x() += (hx*omega_*sin_).sum();
        out.acceleration.y() += (hy*omega_*sin_).sum();
        out.acceleration.z() -= (vz*omega_.square()*cos_).sum();
    }

    void updateTrig(double x, double y, double t) const {
        if (x == cache_x_ && y == cache_y_ && t == cache_t_) return;
        const bool labelsChanged=x != cache_x_ || y != cache_y_;
        mode_sin_.resize(modes_.size()); mode_cos_.resize(modes_.size());
        const Eigen::Vector2d xy(x,y);
        sin_.resize(omega_.size()); cos_.resize(omega_.size());
        integral_cos_.resize(omega_.size());
        if (labelsChanged) {
            for(size_t i=0;i<modes_.size();++i) {
                const auto& m=modes_[i];
                const double arg=m.k*m.direction.dot(xy)+m.phase;
                mode_sin_[i]=std::sin(arg); mode_cos_[i]=std::cos(arg);
            }
            sin0_.resize(omega_.size()); cos0_.resize(omega_.size());
            for(Eigen::Index p=0;p<omega_.size();++p) {
                const auto i=pair_i_[p],j=pair_j_[p]; const int sign=pair_sign_[p];
                sin0_(p)=mode_sin_[i]*mode_cos_[j]+sign*mode_cos_[i]*mode_sin_[j];
                cos0_(p)=mode_cos_[i]*mode_cos_[j]-sign*mode_sin_[i]*mode_sin_[j];
            }
        }
        // Angle-addition identities avoid O(N^2) transcendental calls. They
        // preserve the complete pair set; no frequency or amplitude is dropped.
        for(size_t i=0;i<modes_.size();++i) {
            const auto& m=modes_[i];
            const double arg=m.k*m.direction.dot(xy)-m.omega*t+m.phase;
            mode_sin_[i]=std::sin(arg); mode_cos_[i]=std::cos(arg);
        }
        for(Eigen::Index p=0;p<omega_.size();++p) {
            const auto i=pair_i_[p],j=pair_j_[p]; const int sign=pair_sign_[p];
            sin_(p)=mode_sin_[i]*mode_cos_[j]+sign*mode_cos_[i]*mode_sin_[j];
            cos_(p)=mode_cos_[i]*mode_cos_[j]-sign*mode_sin_[i]*mode_sin_[j];
            const double half=0.5*omega_(p)*t;
            if (std::abs(half)<1e-4) {
                const double h2=half*half;
                const double sinc=1-h2/6+h2*h2/120;
                const double cmid=cos0_(p)*(1-h2/2+h2*h2/24)+sin0_(p)*half*sinc;
                integral_cos_(p)=t*sinc*cmid;
            } else {
                integral_cos_(p)=(sin0_(p)-sin_(p))/omega_(p);
            }
        }
        cache_x_=x; cache_y_=y; cache_t_=t;
    }
};
