#!/usr/bin/env python3

import os

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

hz = 50  # in Hz
jitter = 5000  # in uS

lock_thresh = 0.008
lock_thresh_count = 100

# unlocked (fastlock mode)
b_l_u = 0.2  # in Hz
zeta_u = 0.707
# locked (maintenance mode)
b_l_l = 0.005  # in Hz
zeta_l = 0.707


t_s = 1 / hz
kp_u, ki_u = calculate_pll_coeffs(t_s, b_l_u, zeta_u)
kp_l, ki_l = calculate_pll_coeffs(t_s, b_l_l, zeta_l)

print(f'// hz = {hz} Hz')
print(f'// bandwidth_u = {b_l_u} Hz')
print(f'// zeta_u = {zeta_u}')
print(f'// bandwidth_l = {b_l_l} Hz')
print(f'// zeta_l = {zeta_l}')
print('')
print('// NOTE: this is in sec')
print(f"#define INTERVAL {t_s}")
print('')
print('// NOTE: this is in usec')
print(f"#define JITTER {jitter}")
print('')
print('// loop params')
print(f"#define K_P_U {kp_u}")
print(f"#define K_I_U {ki_u}")
print(f"#define K_P_L {kp_l}")
print(f"#define K_I_L {ki_l}")
print('')
print(f'#define LOCK_THRESH {lock_thresh}')
print(f'#define LOCK_THRESH_COUNT {lock_thresh_count}')

gid = os.getgid()
uid = os.getuid()

print('')
print(f'#define UNPRIV_GID  {gid}')
print(f'#define UNPRIV_UID  {uid}')
