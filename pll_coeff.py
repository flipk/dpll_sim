def calculate_pll_coeffs(clock_period, loop_bandwidth, zeta):
    # Calculate natural frequency (omega_n) from loop bandwidth (B_L)
    # Note: B_L is in Hz, omega_n is in rad/s
    omega_n = (4 * loop_bandwidth) / (zeta + 1 / (4 * zeta))
    
    # Calculate the product of natural frequency and sampling period
    wt = omega_n * clock_period
    
    # Calculate the common denominator for the bilinear transform
    denominator = 4 + (4 * zeta * wt) + (wt**2)
    
    # Calculate Proportional (K_p) and Integral (K_i) coefficients
    k_p = (4 * zeta * wt) / denominator
    k_i = (4 * (wt**2)) / denominator
    
    return k_p, k_i

hz = 40
b_l = 0.1
jitter = 5000

zeta = 0.707

t_s = 1 / hz
kp, ki = calculate_pll_coeffs(t_s, b_l, zeta)
print(f'// hz = {hz} Hz')
print(f'// bandwidth = {b_l} Hz')
print('')
print('// NOTE: this is in sec')
print(f"#define INTERVAL {t_s}")
print('')
print('// NOTE: this is in usec')
print(f"#define JITTER {jitter}")
print('')
print('// loop params')
print(f"#define K_P {kp}")
print(f"#define K_I {ki}")
