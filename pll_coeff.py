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

lock_thresh_count = 100
lock_thresh_12 = 0.01
lock_thresh_23 = 0.001

# stage 1 (fastlock mode)
b_l_1 = 0.2  # in Hz
zeta_1 = 0.707
# stage 2
b_l_2 = 0.05  # in Hz
zeta_2 = 0.707
# stage 3
b_l_3 = 0.002  # in Hz
zeta_3 = 0.950


t_s = 1 / hz
kp_1, ki_1 = calculate_pll_coeffs(t_s, b_l_1, zeta_1)
kp_2, ki_2 = calculate_pll_coeffs(t_s, b_l_2, zeta_2)
kp_3, ki_3 = calculate_pll_coeffs(t_s, b_l_3, zeta_3)

print(f'// hz = {hz} Hz')
print('')
print('// NOTE: this is in sec')
print(f"#define INTERVAL {t_s}")
print('')
print('// NOTE: this is in usec')
print(f"#define JITTER {jitter}")
print('')
print(f'// stage 1 (fastlock), bandwidth = {b_l_1} zeta = {zeta_1}')
print(f"#define K_P_1 {kp_1}")
print(f"#define K_I_1 {ki_1}")
print(f'// stage 2 bandwidth = {b_l_2} zeta = {zeta_2}')
print(f"#define K_P_2 {kp_2}")
print(f"#define K_I_2 {ki_2}")
print(f'// stage 3 (maintenance), bandwidth = {b_l_3} zeta = {zeta_3}')
print(f"#define K_P_3 {kp_3}")
print(f"#define K_I_3 {ki_3}")
print('')
print(f'#define LOCK_THRESH_COUNT {lock_thresh_count}')
print(f'#define LOCK_THRESH_12 {lock_thresh_12}')
print(f'#define LOCK_THRESH_23 {lock_thresh_23}')

gid = os.getgid()
uid = os.getuid()

print('')
print(f'#define UNPRIV_GID  {gid}')
print(f'#define UNPRIV_UID  {uid}')
