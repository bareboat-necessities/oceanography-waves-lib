# JONSWAP particle kinematics through second order

`Jonswap3dStokesWaves` uses a directional deep-water potential-flow expansion
through second order in wave amplitude. `DeepWaterSecondOrder` implements the
quadratic wave interactions and the particle advection correction. The
Pierson–Moskowitz `PMStokesN3dWaves` implementation is a separate, unchanged
per-component harmonic approximation; the validation here does not promote it
to the same physical model.

This replaces the former angle-independent sum-frequency coefficient
`ki*kj/(ki+kj)`, reversed horizontal particle orbit, and inconsistent signs
between horizontal quadratic displacement and its time derivatives. It also
replaces the mirrored phases of different frequencies with independent seeded
phases. No acceleration clamp or empirical attenuation is applied.

## Coordinates and API

- Gravity points along negative world Z; `acceleration` excludes gravity.
  An accelerometer measures `C_wb * (acceleration - gravity_world)`.
- In `getLagrangianState(x,y,t,z)`, `x,y,z` are particle labels. `z=0`
  labels a surface particle and `z<0` a subsurface particle. Displacement is
  relative to that label, not necessarily relative to the position at `t=0`.
- Horizontal second-order displacement is integrated from `t=0`. This fixes
  its otherwise arbitrary integration constant and includes the secular
  Stokes-drift displacement. Position, velocity and acceleration now describe
  the same trajectory. The vertical map includes the Lagrangian mean setup.
- `getSurfaceState` remains an alias for the **surface particle** state.
  `getSurfaceElevation` returns Eulerian elevation at fixed horizontal coordinates.
- `getSurfaceSlopes` is the Eulerian spatial gradient. To obtain the gradient
  seen by a surface particle, use `getLagrangianSurfaceSlopes`. The IMU and
  Euler-angle methods, and the CSV reference quaternion, use this latter API.
  Do not evaluate Eulerian slopes at the drifted particle position as a
  replacement: that mixes in uncontrolled higher-order terms over long records.
- The constructor's `cutoff_tol` argument is retained for source compatibility,
  validated, and no longer prunes interactions. A displacement-only cutoff is
  not an acceleration-error bound. All nonzero-amplitude pairs are retained.
- `Hs` normalizes the **linear** spectrum to variance `Hs^2/16`. It does not
  renormalize the nonlinear particle trajectory back to that variance.
- The spectrum, frequency band and seed arguments are unchanged. Numerical
  realizations intentionally change with the corrected orbit, pair kernels,
  drift integration and independent phases.

## Derivation of the interaction kernels

Use the cosine convention internally, with propagation direction `d_i`,
`k_i = omega_i^2/g`, and

\[
\theta_i=k_i\mathbf d_i\cdot\mathbf X-\omega_i t+\phi_i,\quad
\eta_1=\sum_i a_i\cos\theta_i,\quad
\Phi_1=\sum_i\frac{g a_i}{\omega_i}e^{k_i z}\sin\theta_i.
\]

The wrapper shifts its sine-convention phase by `-pi/2` when constructing the
kernel. The first-order particle displacement is

\[
\boldsymbol\xi_{1h}=-\sum_i a_i e^{k_i z}\mathbf d_i\sin\theta_i,
\qquad \xi_{1z}=\sum_i a_i e^{k_i z}\cos\theta_i.
\]

This gives fluid velocity in the propagation direction under a crest.

At still water, the second-order free-surface equations are

\[
\partial_t\eta_2-\partial_z\Phi_2
=\eta_1\partial_{zz}\Phi_1-\nabla_h\eta_1\cdot\nabla_h\Phi_1,
\]
\[
\partial_t\Phi_2+g\eta_2
=-\eta_1\partial_{tz}\Phi_1-\tfrac12|\nabla\Phi_1|^2.
\]

For each `i <= j`, evaluate **both** signs `s=+1,-1`. Define

\[
A=m_{ij}a_i a_j,\quad m_{ii}=\tfrac12,\quad m_{ij}=1\ (i<j),
\quad c=\mathbf d_i\cdot\mathbf d_j,
\]
\[
\Omega=\omega_i+s\omega_j,\quad
\mathbf q=k_i\mathbf d_i+s k_j\mathbf d_j,\quad K=|\mathbf q|,
\quad\Theta=\theta_i+s\theta_j.
\]

The pair contributes `E cos(Theta)` to Eulerian elevation and
`P exp(Kz) sin(Theta)` to potential. Solving the two boundary equations gives

\[
D=\tfrac A2[\omega_i^2+\omega_j^2-(c-s)\omega_i\omega_j],
\quad
P=\frac{A\Omega(c-s)\omega_i\omega_j}{\Omega^2-gK},
\quad E=\frac{D+\Omega P}{g}.
\]

The Eulerian potential must satisfy Laplace's equation, hence its depth factor
is `exp(|q| z)`, not `exp((ki+kj) z)` for an arbitrary directional interaction.

The Lagrangian velocity correction follows by expanding the Eulerian velocity
at the particle location:

\[
\dot{\boldsymbol\xi}_2=\nabla\Phi_2+
  (\boldsymbol\xi_1\cdot\nabla)\nabla\Phi_1.
\]

Write its horizontal coefficient as

\[
\mathbf H(z)=P\mathbf q e^{Kz}
+\tfrac A2(1-sc)(k_i\omega_i\mathbf d_i+k_j\omega_j\mathbf d_j)
 e^{(k_i+k_j)z},
\]

and its vertical displacement coefficient as

\[
Z(z)=A(c-s)\omega_i\omega_j\frac{K}{\Omega^2-gK}e^{Kz}
+\frac{A(1-sc)}{2g}
 (\omega_i^2-s\omega_i\omega_j+\omega_j^2)e^{(k_i+k_j)z}.
\]

Then a pair contributes

\[
\xi_{2h}(t)=\mathbf H\int_0^t\cos(\Theta(0)-\Omega u)\,du,
\quad \xi_{2z}=Z\cos\Theta,
\]
\[
\dot\xi_{2h}=\mathbf H\cos\Theta,
\quad\dot\xi_{2z}=\Omega Z\sin\Theta,
\quad\ddot\xi_{2h}=\Omega\mathbf H\sin\Theta,
\quad\ddot\xi_{2z}=-\Omega^2 Z\cos\Theta.
\]

The cosine integral uses a sinc limit near zero frequency. Difference-frequency
denominators use a rationalized expression, so nearly coincident wavevectors
do not trigger a numerical division by a rounded zero. Exactly coincident
wavevectors use the analytic diagonal:

\[
\mathbf H=2Ak_i\omega_i\mathbf d_i e^{2k_i z},\qquad
Z=Ak_i e^{2k_i z},\qquad E=P=0.
\]

This includes coherent cross terms when a component is split into identical
components. Equal frequencies at different directions retain their static
spatial interference terms instead of being discarded.

The surface slope pulled back to a particle label is truncated consistently:

\[
\nabla_h\eta|_{\rm particle}
=\nabla_h\eta_1+\nabla_h\eta_2+
  (\nabla_h\nabla_h\eta_1)\boldsymbol\xi_{1h}+O(a^3).
\]

## Physical checks

For a single deep-water component, Eulerian elevation contains
`(ka^2/2) cos(2 theta)`. The Lagrangian vertical second harmonic cancels:
the particle has its first-order oscillation, mean vertical setup `ka^2/2`,
and horizontal Stokes drift `omega*k*a^2`. For two collinear components,
the particle's vertical quadratic oscillation is instead

\[
a_i a_j\min(k_i,k_j)\cos(\theta_i-\theta_j).
\]

The regression suite checks these limits, component-splitting invariance,
both free-surface boundary equations independently by differentiating the
Eulerian potential, the Eulerian-to-Lagrangian velocity relation at two depths,
surface membership through second order, position/velocity derivatives and
IMU attitude/specific-force consistency. It does not assert that arbitrary
wave accelerations must be below gravity.

## Scope and downstream migration

This is a weakly nonlinear, deep-water, nonbreaking, irrotational wave model.
It includes all quadratic interactions of its finite input spectrum, but is
not a fully nonlinear solution, breaking/slamming model, or vessel response
model. The existing per-component steepness check is only a coarse input guard;
passing it is not physical qualification of an entire broadband realization.
`linearRmsSlope()` exposes the full-spectrum linear RMS slope for auditing.
High-frequency cutoff sensitivity and higher-order effects still require
qualification for any deployment envelope.

As with a fixed-label perturbation expansion, secular drift makes long-time
absolute particle position and phase a nonuniform approximation. The model
does not claim uniformly accurate particle tracking for arbitrarily long
times. Consistent order-truncated particle slopes avoid feeding secular drift
back into an Eulerian field as though that were a higher-order solution.

Consumers must distinguish total horizontal displacement (now including drift)
from detrended wave excursions. Constant velocity is unobservable from
acceleration alone. Existing estimator position-error comparisons and attitude
reference construction therefore need review when updating this dependency.
No ocean-imu filter or BRMM bounds are changed here. This library has no `Live`
state: its full-record audit cannot establish new pre-Live/Live classifications.

## References

- Herbers and Janssen (2016), *Lagrangian Surface Wave Motion and Stokes Drift
  Fluctuations*, [doi:10.1175/JPO-D-15-0129.1](https://doi.org/10.1175/JPO-D-15-0129.1).
- McAllister and van den Bremer (2019), *Lagrangian Measurement of Steep
  Directionally Spread Ocean Waves*,
  [doi:10.1175/JPO-D-19-0170.1](https://doi.org/10.1175/JPO-D-19-0170.1).
- McAllister and van den Bremer (2020), *Experimental Study of the Statistical
  Properties of Directionally Spread Ocean Waves Measured by Buoys*,
  [doi:10.1175/JPO-D-19-0228.1](https://doi.org/10.1175/JPO-D-19-0228.1).
  Their discussion distinguishes cancellation of Lagrangian superharmonics from
  the accompanying increase in subharmonics; it does not justify deleting all
  quadratic motion.

The formulas above are derived from the stated boundary and particle equations;
the code does not reproduce an external implementation or claim validation
against a separate numerical solver.
