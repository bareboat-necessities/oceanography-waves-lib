// Included in the test runner namespace.
void test_vessel_transfer() {
    const VesselRao vessel({});
    const auto p=vessel.parameters();
    for (int feet : {28,34,42,50}) {
        const double ratio=feet/28.0;
        const VesselRao scaled({},VesselRao::sailboat(feet));
        for (double omega : {0.1,0.8,2.0,5.0}) {
            const auto base=vessel.transfer(omega,omega*omega/p.gravity,0.7);
            const auto other=scaled.transfer(omega/std::sqrt(ratio),omega*omega/p.gravity/ratio,0.7);
            for (int j=0;j<6;++j)
                require(std::abs(other[j]*(j<3?1.0:ratio)-base[j])<1e-12,
                        "Froude similarity preserves translation and scales rotation per wave amplitude");
        }
    }
    bool invalid_size=false;
    try { VesselRao::sailboat(35); } catch (const std::invalid_argument&) { invalid_size=true; }
    require(invalid_size,"Unsupported vessel size rejected");

    const double pi=std::acos(-1.0);
    for (double beta : {0.0,pi/2,0.6,-0.6}) {
        const double w=1e-6, k=w*w/p.gravity;
        const auto h=vessel.transfer(w,k,beta);
        require(std::abs(h[0]-std::complex<double>(0,std::cos(beta)))<2e-6,"Long-wave surge phase");
        require(std::abs(h[1]-std::complex<double>(0,std::sin(beta)))<2e-6,"Long-wave sway phase");
        require(std::abs(h[2]-1.0)<1e-6,"Long-wave heave follows elevation");
        require(std::abs(h[3]/k-std::complex<double>(0,std::sin(beta)))<1e-6,"Long-wave roll follows slope");
        require(std::abs(h[4]/k-std::complex<double>(0,-std::cos(beta)))<1e-6,"Long-wave pitch follows slope");
        for (const auto v : vessel.transfer(100,10000/p.gravity,beta))
            require(std::abs(v)<1e-9,"Short-wave displacement response vanishes");
    }
    require(std::abs(vessel.transfer(1,1/p.gravity,0)[3])<1e-14,"No roll in head/following waves");
    require(std::abs(vessel.transfer(1,1/p.gravity,pi/2)[4])<1e-14,"No pitch in beam waves");
    const double w=2*pi/p.heave_period,k=w*w/p.gravity;
    const double footprint=std::exp(-std::pow(k*p.waterline_length,2)/24);
    const auto h=vessel.transfer(w,k,0)[2];
    require_near(h.real(),0,0,1e-14,"At resonance heave is in quadrature");
    require_near(h.imag(),footprint/(2*p.heave_damping),1e-12,1e-14,"Damped heave resonant response");
    const VesselRao single({{0.2,w,k,0,0}});
    require_near(single.state(pi/(2*w)).displacement.z(),0.2*h.imag(),1e-12,1e-14,
                 "With exp(-iwt), positive response phase produces a time lag");
    auto invalid=p;
    invalid.roll_damping=0;
    bool threw=false;
    try { const VesselRao bad({},invalid); } catch (const std::invalid_argument&) { threw=true; }
    require(threw,"Undamped singular resonance must be rejected");
}

void test_vessel_kinematics() {
    VesselRao::Parameters p;
    p.heading=0.4;
    const VesselRao vessel({{0.4,1.3,1.3*1.3/p.gravity,0.9,0.7},
                            {0.15,2.1,2.1*2.1/p.gravity,-0.5,1.8}},p);
    constexpr double dt=1e-5;
    for (double t : {0.0,0.7,3.9,17.4}) {
        const auto st=vessel.state(t),lo=vessel.state(t-dt),hi=vessel.state(t+dt);
        require(((hi.displacement-lo.displacement)/(2*dt)-st.velocity).norm()<1e-8,
                "Vessel velocity differentiates displacement");
        require(((hi.velocity-lo.velocity)/(2*dt)-st.acceleration).norm()<1e-8,
                "Vessel acceleration differentiates velocity");
        const Eigen::Matrix3d rate=(hi.world_to_body-lo.world_to_body)/(2*dt);
        const Eigen::Matrix3d skew=-rate*st.world_to_body.transpose();
        const Eigen::Vector3d gyro(skew(2,1),skew(0,2),skew(1,0));
        require((gyro-st.gyro_body).norm()<1e-8,"Body gyro differentiates full vessel attitude");
        require((st.world_to_body.transpose()*st.accel_body-Eigen::Vector3d(0,0,p.gravity)
                 -st.acceleration).norm()<1e-12,"Specific force recovers world CG acceleration");
        const Eigen::Quaterniond q(st.world_to_body);
        require_near(q.norm(),1,0,1e-12,"Reference quaternion is unit length");
        const Eigen::Matrix3d expected=(Eigen::AngleAxisd(st.euler.z(),Eigen::Vector3d::UnitZ())*
            Eigen::AngleAxisd(st.euler.y(),Eigen::Vector3d::UnitY())*
            Eigen::AngleAxisd(st.euler.x(),Eigen::Vector3d::UnitX())).toRotationMatrix().transpose();
        require((expected-q.toRotationMatrix()).norm()<1e-12,"Euler and quaternion conventions agree");
    }
    const auto calm=VesselRao({}).state(123);
    require_near(calm.accel_body.z(),p.gravity,0,1e-12,"Stationary accelerometer reports +g");
    require(calm.gyro_body.isZero() && calm.acceleration.isZero(),"Calm sea has no dynamics");
}

void test_incident_wave_adapters() {
    auto elevation=[](const std::vector<WaveHarmonic>& waves,double x,double y,double t) {
        double eta=0;
        for (const auto& w : waves)
            eta+=w.amplitude*std::cos(w.wavenumber*(x*std::cos(w.direction)+y*std::sin(w.direction))
                                      -w.omega*t+w.phase);
        return eta;
    };
    auto dist=std::make_shared<Cosine2sRandomizedDistribution>(0.3,10,42);
    const Jonswap3dStokesWaves<16> j(1e-5,6,dist);
    const PMStokesN3dWaves<16,1> pm(1,6,dist);
    for (double t : {0.0,0.3,7.1}) {
        require_near(elevation(j.incidentHarmonics(),1,2,t),j.getSurfaceElevation(1,2,t),
                     0,2e-10,"JONSWAP adapter retains actual first-order phases and directions");
        require_near(elevation(pm.incidentHarmonics(),1,2,t),pm.getEulerianState(1,2,0,t).displacement.z(),
                     1e-12,1e-12,"PM adapter retains actual first-order phases and directions");
    }
    for (const auto wp : std::vector<WaveParameters>{{3,.27f,1,0},{5.7f,1.5f,2,0},
                                                    {8.5f,4,.5f,0},{11.4f,8.5f,1.2f,0}}) {
        const auto fp=FentonWave<5>::infer_fenton_parameters_from_amplitude(
            wp.height/2.0f,200.0f,2.0f*M_PI/wp.period,wp.phase);
        const FentonWave<5> fenton(fp.height,fp.depth,fp.length);
        const CnoidalWave<float> cnoidal(200,wp.height/2,wp.period,0,g_std);
        const auto f=regularWaveHarmonics(WaveType::FENTON,wp);
        const auto c=regularWaveHarmonics(WaveType::CNOIDAL,wp);
        const auto g=regularWaveHarmonics(WaveType::GERSTNER,wp);
        const double omega=2*M_PI/wp.period,k=omega*omega/g_std,a=wp.height/2;
        for (double t : {0.0,0.31,3.79,9.8}) {
            require_near(elevation(f,0,0,t),fenton.surface_elevation(fp.initial_x,t)-fenton.E(0),
                         0,2e-5,"Fenton incident harmonics reconstruct the surface");
            require_near(elevation(c,0,0,t),cnoidal.surfaceElevation(0,0,t),
                         0,2e-5,"Cnoidal harmonics preserve its actual period and amplitude");
            // Check at known particle labels, so no second inversion is used by the test.
            const double q=0.37,phase=k*q-omega*t+wp.phase,x=q-a*std::sin(phase);
            // Gerstner's Fourier representation excludes the mean datum a^2*k/2.
            require_near(elevation(g,x,0,t),-a*std::cos(phase)-a*a*k/2,
                         0,1e-10,"Gerstner harmonics reconstruct its Eulerian surface");
        }
    }
}
