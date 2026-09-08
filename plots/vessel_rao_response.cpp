// Sample the exact production transfer function; no duplicated RAO equations.
#define EIGEN_NON_ARDUINO
#include "VesselRao.h"
#include <iomanip>
#include <iostream>

int main() {
    const double pi = std::acos(-1.0);
    std::cout << "feet,frequency_hz,beta_deg,dof,real,imag\n" << std::setprecision(17);
    for (int feet : {28, 34, 42, 50}) {
        const VesselRao vessel({}, VesselRao::sailboat(feet));
        for (int fi = 1; fi <= 200; ++fi) {
            const double frequency = fi * 0.005, omega = 2*pi*frequency;
            for (int beta = 0; beta <= 180; beta += 5) {
                const auto h = vessel.transfer(omega, omega*omega/vessel.parameters().gravity,
                                               beta*pi/180);
                for (int j = 0; j < 6; ++j)
                    std::cout << feet << ',' << frequency << ',' << beta << ',' << j
                              << ',' << h[j].real() << ',' << h[j].imag() << '\n';
            }
        }
    }
}
