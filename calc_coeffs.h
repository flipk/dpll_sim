#define IDEAL_ZETA 0.707
void calc_coeffs(double & k_p, // return
                 double & k_i, // return
                 double clock_period,
                 double loop_bandwidth,
                 double zeta = IDEAL_ZETA);
