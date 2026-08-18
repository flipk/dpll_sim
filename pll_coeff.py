#!/usr/bin/env python3

import os

gid = os.getgid()
uid = os.getuid()

hz = 50  # in Hz
clock_period = 1 / hz

# if jitter is lower, then lock threshold should be higher.
jitter = 1000  # in uS
lock_thresh_count = 200

print(f'#define HZ {hz}\n'
      f'#define INTERVAL {clock_period} // in seconds\n'
      f'#define JITTER {jitter} // in usec\n'
      f'#define LOCK_THRESH_COUNT {lock_thresh_count}\n'
      f'#define UNPRIV_GID  {gid}\n'
      f'#define UNPRIV_UID  {uid}')

class StageParams:
    def __init__(self,
                 loop_bandwidth: float,
                 zeta: float,
                 lock_thresh: float):
        self.loop_bandwidth = loop_bandwidth
        self.zeta = zeta
        self.lock_thresh = lock_thresh
        self.calc_coeffs()

    def calc_coeffs(self):
        global clock_period

        # Calculate natural frequency (omega_n) from loop bandwidth (B_L)
        # Note: B_L is in Hz, omega_n is in rad/s
        omega_n = (4 * self.loop_bandwidth) / \
                  (self.zeta + 1 / (4 * self.zeta))
    
        # Calculate the product of natural frequency and sampling period
        wt = omega_n * clock_period
    
        # Calculate the common denominator for the bilinear transform
        denominator = 4 + (4 * self.zeta * wt) + (wt**2)

        # Calculate Proportional (K_p) and Integral (K_i) coefficients
        self.k_p = (4 * self.zeta * wt) / denominator
        self.k_i = (4 * (wt**2)) / denominator

params = [
    StageParams(0.200, 0.707, 0.0200),
    StageParams(0.050, 0.707, 0.0200),
    StageParams(0.020, 0.707, 0.0200),
    StageParams(0.010, 0.900, 0.0400)
]

print('struct StageParams {\n'
      '   double k_p;\n'
      '   double k_i;\n'
      '   double lock_thresh;\n'
      '};\n'
      'static StageParams stage_params[] = {')

stage = 0
for p in params:
    print(f'   // stage {stage+1} bw = {p.loop_bandwidth} zeta = {p.zeta}\n'
          '   {'f' {p.k_p:12e}, {p.k_i:12e}, {p.lock_thresh} ''},')
    stage += 1

print('};\n'
      f'#define NUM_STAGES {stage}')
