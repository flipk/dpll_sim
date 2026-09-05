#if 0
set -e -x
g++ calc_coeffs.cc -D__TEST_MAIN__ -o calc_coeffs
./calc_coeffs
rm -f calc_coeffs
exit 0
#endif

#include "calc_coeffs.h"

#include <math.h>
#include <stdio.h>

void calc_coeffs(double & k_p, // return
                 double & k_i, // return
                 double clock_period,
                 double loop_bandwidth,
                 double zeta)
{
    double omega_n, wt, denominator;

    // Calculate natural frequency (omega_n) from loop bandwidth (B_L)
    // Note: B_L is in Hz, omega_n is in rad/s
    omega_n =
        (4.0 * loop_bandwidth) /
        (zeta + 1.0 / (4.0 * zeta));

    // Calculate the product of natural frequency and sampling period
    wt = omega_n * clock_period;
    
    // Calculate the common denominator for the bilinear transform
    denominator = 4.0 + (4.0 * zeta * wt) + pow(wt, 2);

    // Calculate Proportional (K_p) and Integral (K_i) coefficients
    k_p = (4.0 * zeta * wt) / denominator;
    k_i = (4.0 * pow(wt,2)) / denominator;
}

#ifdef __TEST_MAIN__

int main()
{
    double clock_period = 0.01;
    double loop_bandwidth = 0.2000;
    double zeta = 0.707;
    double k_p, k_i;

    calc_coeffs(k_p, k_i, clock_period, loop_bandwidth, zeta);

    printf("k_p = %12e   k_i = %12e\n", k_p, k_i);
    return 0;
}



#endif // __TEST_MAIN__
