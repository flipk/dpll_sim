#!/usr/bin/env python3

import os

gid = os.getgid()
uid = os.getuid()

hz = 50  # in Hz
clock_period = 1 / hz

# if jitter is lower, then lock threshold should be higher.
jitter = 1000  # in uS

print(f'#define HZ {hz}\n'
      f'#define INTERVAL {clock_period} // in seconds\n'
      f'#define JITTER {jitter} // in usec\n'
      f'#define UNPRIV_GID  {gid}\n'
      f'#define UNPRIV_UID  {uid}')

class StageParams:
    def __init__(self,
                 loop_bandwidth: float,
                 zeta: float,
                 lock_thresh: float,
                 lock_count: int,
                 unlock_count: int):
        self.loop_bandwidth = loop_bandwidth
        self.zeta = zeta
        self.lock_thresh = lock_thresh
        self.lock_count = lock_count
        self.unlock_count = unlock_count
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
    StageParams(0.200, 0.707, 0.0200, 200, 9999),
    StageParams(0.050, 0.707, 0.0200, 200, 5000),
    StageParams(0.030, 0.707, 0.0200, 200, 5000),
    StageParams(0.015, 0.707, 0.1000, 200, 5000)
]

print('struct StageParams {\n'
      '   double k_p;\n'
      '   double k_i;\n'
      '   double lock_thresh;\n'
      '   int lock_thresh_count;\n'
      '   int unlock_thresh_count;\n'
      '};\n'
      'static StageParams stage_params[] = {')

stage = 0
for p in params:
    print(f'   // stage {stage+1} bw = {p.loop_bandwidth} zeta = {p.zeta}\n'
          '   {\n'
          f'      {p.k_p:12e}, {p.k_i:12e}, {p.lock_thresh}, '
          f'{p.lock_count}, {p.unlock_count} \n'
          '},')
    stage += 1

print('};\n'
      f'#define NUM_STAGES {stage}')
