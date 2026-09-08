// Included inside tests.cpp's test namespace; uses its assertion helpers.
using WaveComponent = DeepWaterSecondOrder::Component;

void require_vector(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected,
                    double tolerance, const std::string& message) {
  require_near((actual-expected).norm(),0,0,tolerance,message);
}

void test_monochromatic_particle() {
  constexpr double g=9.80665, a=0.3, w=1.2, k=w*w/g, direction=0.7, phase=0.4;
  DeepWaterSecondOrder wave({{a,w,direction,phase}},g);
  const Eigen::Vector2d d(std::cos(direction),std::sin(direction));
  for (double z : {0.0,-0.7}) for (double t : {0.0,0.6,1200.0}) {
    const double theta=k*d.dot(Eigen::Vector2d(0.2,-0.1))-w*t+phase;
    const double az=a*std::exp(k*z), drift=w*k*az*az;
    Eigen::Vector3d position,velocity,acceleration;
    position.head<2>()=(-az*std::sin(theta)+drift*t)*d;
    position.z()=az*std::cos(theta)+0.5*k*az*az;
    velocity.head<2>()=(az*w*std::cos(theta)+drift)*d;
    velocity.z()=az*w*std::sin(theta);
    acceleration.head<2>()=az*w*w*std::sin(theta)*d;
    acceleration.z()=-az*w*w*std::cos(theta);
    const auto state=wave.state(0.2,-0.1,t,z);
    require_vector(state.displacement,position,2e-12,"Monochromatic orbit and integrated Stokes drift");
    require_vector(state.velocity,velocity,2e-12,"Monochromatic particle velocity");
    require_vector(state.acceleration,acceleration,2e-12,"No spurious monochromatic second harmonic acceleration");
    if (z == 0)
      require_near(wave.surfaceElevation(0.2,-0.1,t),
          a*std::cos(theta)+0.5*k*a*a*std::cos(2*theta),0,2e-12,
          "Eulerian Stokes crest correction retains correct phase");
  }
}

void test_collinear_and_degenerate_pairs() {
  constexpr double g=9.80665, a=0.2, b=0.13, wi=1.3, wj=0.8;
  DeepWaterSecondOrder wave({{a,wi,0,0.2},{b,wj,0,-0.4}},g);
  for (double t : {0.0,1.0,5.0}) {
    const double ti=0.2-wi*t, tj=-0.4-wj*t;
    const double expected=a*std::cos(ti)+b*std::cos(tj)+
        0.5*(a*a*wi*wi+b*b*wj*wj)/g+a*b*wj*wj/g*std::cos(ti-tj);
    require_near(wave.state(0,0,t).displacement.z(),expected,0,2e-12,
        "Collinear surface particle has difference-frequency setup, no sum harmonics");
  }
  DeepWaterSecondOrder single({{a+b,wi,0.4,0.2}},g);
  DeepWaterSecondOrder split({{a,wi,0.4,0.2},{b,wi,0.4,0.2}},g);
  for (double z : {0.0,-1.0}) {
    const auto one=single.state(0.3,0.5,100,z), two=split.state(0.3,0.5,100,z);
    require_vector(one.displacement,two.displacement,2e-12,"Identical-component splitting preserves the orbit");
    require_vector(one.velocity,two.velocity,2e-12,"Zero-frequency cross terms include coherent drift");
  }
  // The displacement integral must have a finite zero-frequency limit.
  DeepWaterSecondOrder near({{a,wi,0.4,0.2},{b,wi+1e-12,0.4,0.2}},g);
  require_vector(near.state(0,0,3).displacement,single.state(0,0,3).displacement,
                 1e-10,"Nearly equal frequencies converge without cancellation or division by zero");
}

struct LinearFields {
  double eta=0, phi=0, phi_zz=0, phi_tz=0;
  Eigen::Vector2d slope=Eigen::Vector2d::Zero();
  Eigen::Vector3d velocity=Eigen::Vector3d::Zero(), xi=Eigen::Vector3d::Zero();
  Eigen::Matrix3d velocity_gradient=Eigen::Matrix3d::Zero();
};

LinearFields linear_fields(const std::vector<WaveComponent>& components,
                           double x,double y,double z,double t,double g) {
  LinearFields f;
  for (const auto& m : components) {
    const double k=m.omega*m.omega/g;
    const Eigen::Vector2d d(std::cos(m.direction),std::sin(m.direction));
    const double theta=k*d.dot(Eigen::Vector2d(x,y))-m.omega*t+m.phase;
    const double s=std::sin(theta),c=std::cos(theta),a=m.amplitude*std::exp(k*z);
    f.eta+=m.amplitude*c;
    f.slope-=m.amplitude*k*s*d;
    f.phi+=a*g/m.omega*s;
    f.phi_zz+=a*m.omega*k*s;
    f.phi_tz-=a*m.omega*m.omega*c;
    f.velocity.head<2>()+=a*m.omega*c*d; f.velocity.z()+=a*m.omega*s;
    f.xi.head<2>()-=a*s*d; f.xi.z()+=a*c;
    f.velocity_gradient.topLeftCorner<2,2>()-=a*m.omega*k*s*(d*d.transpose());
    f.velocity_gradient.topRightCorner<2,1>()+=a*m.omega*k*c*d;
    f.velocity_gradient.bottomLeftCorner<1,2>()+=a*m.omega*k*c*d.transpose();
    f.velocity_gradient(2,2)+=a*m.omega*k*s;
  }
  return f;
}

void test_second_order_boundary_conditions() {
  constexpr double g=9.80665, h=2e-5;
  const std::vector<std::vector<WaveComponent>> cases={
    {{0.18,1.3,0.2,0.4},{0.11,0.8,1.1,-0.6},{0.07,1.7,-0.8,0.9}},
    {{0.18,1.3,0,0.4},{0.11,1.3,1.1,-0.6}}, // steady crossing difference term
    {{0.18,1.3,0,0.4},{0.11,1.3,M_PI,-0.6}}, // standing wave / q+=0
    {{0.18,1.3,0,0.4},{0.11,0.8,0,-0.6}}
  };
  for (const auto& modes : cases) {
    DeepWaterSecondOrder wave(modes,g);
    auto phi2=[&](double x,double y,double z,double t) {
      return wave.velocityPotential(x,y,z,t)-linear_fields(modes,x,y,z,t,g).phi;
    };
    auto eta2=[&](double x,double y,double t) {
      return wave.surfaceElevation(x,y,t)-linear_fields(modes,x,y,0,t,g).eta;
    };
    for (double t : {0.0,0.7,2.4}) {
      const double x=0.4,y=-0.2;
      const auto f=linear_fields(modes,x,y,0,t,g);
      const double eta2_t=(eta2(x,y,t+h)-eta2(x,y,t-h))/(2*h);
      const double phi2_z=(phi2(x,y,h,t)-phi2(x,y,-h,t))/(2*h);
      const double phi2_t=(phi2(x,y,0,t+h)-phi2(x,y,0,t-h))/(2*h);
      require_near(eta2_t-phi2_z,f.eta*f.phi_zz-f.slope.dot(f.velocity.head<2>()),0,2e-9,
          "Independent second-order kinematic free-surface boundary condition");
      require_near(phi2_t+g*eta2(x,y,t),-f.eta*f.phi_tz-0.5*f.velocity.squaredNorm(),0,2e-9,
          "Independent second-order Bernoulli free-surface boundary condition");
      require_near(wave.state(x,y,t).displacement.z(),
          wave.surfaceElevation(x,y,t)+f.xi.head<2>().dot(f.slope),0,2e-12,
          "Particle remains on the surface through second order");
      for (double z : {0.0,-0.8}) {
        Eigen::Vector3d grad2;
        grad2 << (phi2(x+h,y,z,t)-phi2(x-h,y,z,t))/(2*h),
                 (phi2(x,y+h,z,t)-phi2(x,y-h,z,t))/(2*h),
                 (phi2(x,y,z+h,t)-phi2(x,y,z-h,t))/(2*h);
        const auto depth=linear_fields(modes,x,y,z,t,g);
        require_vector(wave.state(x,y,t,z).velocity,
            depth.velocity+grad2+depth.velocity_gradient*depth.xi,2e-9,
            "Lagrangian velocity equals Eulerian velocity plus particle advection");
      }
    }
  }
}

void test_jonswap_derivatives_and_imu() {
  auto distribution=std::make_shared<Cosine2sRandomizedDistribution>(-M_PI/6,10,42u);
  auto wave=std::make_unique<Jonswap3dStokesWaves<128>>(
      8.5,double(float(11.4)),distribution,0.02,0.8,3.3,9.80665,42u);
  constexpr double h=2e-4;
  for (double t : {0.0,0.4,270.579993952,1199.0}) for (double z : {0.0,-0.3}) {
    const auto s=wave->getLagrangianState(0.1,-0.2,t,z);
    const auto before=wave->getLagrangianState(0.1,-0.2,t-h,z);
    const auto after=wave->getLagrangianState(0.1,-0.2,t+h,z);
    require_vector((after.displacement-before.displacement)/(2*h),s.velocity,3e-6,
                   "JONSWAP d(position)/dt = velocity, including drift");
    require_vector((after.velocity-before.velocity)/(2*h),s.acceleration,3e-6,
                   "JONSWAP d(velocity)/dt = acceleration");
    if (z == 0) {
      require_vector(wave->getSurfaceState(0.1,-0.2,t).displacement,s.displacement,0,
                     "Surface alias uses the same particle map");
      const auto slopes=wave->getLagrangianSurfaceSlopes(0.1,-0.2,t);
      const auto rotation=wave->orientationFromSlopes(slopes);
      const auto imu=wave->getIMUReadings(0.1,-0.2,t,0,h);
      require_vector(rotation.transpose()*imu.accel_body,
                     s.acceleration+Eigen::Vector3d(0,0,9.80665),2e-11,
                     "IMU specific force uses the same particle attitude and acceleration");
      const Eigen::Vector3d angles=wave->getEulerAngles(0.1,-0.2,t)*M_PI/180;
      const Eigen::Matrix3d expected=(Eigen::AngleAxisd(angles.y(),Eigen::Vector3d::UnitY())*
                                     Eigen::AngleAxisd(angles.x(),Eigen::Vector3d::UnitX())).matrix();
      require_near((rotation.transpose()-expected).norm(),0,0,2e-12,
                   "Euler angles and quaternion convention match the IMU attitude");
    }
  }
}
