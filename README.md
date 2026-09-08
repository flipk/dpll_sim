
# Digital Phase Locked Loop Simulator

This is a software simulation of a `Digital Phase Locked Loop`.

The scenario which triggered this project was a hobbyist project
discovered online, in which the author attempted to discipline a 1PPS
oscillator by using NTP over wifi. He encountered significant jitter
but was trying to use a form of proportional integrator to average out
the jitter and recover the original high precision timing.  He never
got it to work, but I believed it was due to errors in configuration
of the loop parameters. He was experimenting in the dark, just trying
stuff, rather than understanding the math behind the scenario.

This tool simulates such an environment, but using the math correctly.

Someday it might be nice to see this in an FPGA, but there are
caveats with that, too-- see the note on FPGA implementation below.


## A note on the limits

This tool is written for linux, tested on Fedora, Ubuntu, and WSL
(Ubuntu) environments. As such, since it is using `struct timespec`
with calls like `clock_nanosleep` and `clock_gettime`, the absolute
best possible resolution of time is 1 nanosecond, and even that is
subject to the whims of the linux kernel (or in WSL's case, the
Windows kernel). Scheduling more precisely than 1 nanosecond is
not possible, and measuring more precisely than 1 nanosecond is
possible but only with levels of effort not pursued in this
simulation.

When running this code, sometimes it seems like it gets precision
better than that -- but be aware that is just due to averaging of
lots of samples. The tick-to-tick interval is NOT that precise.


## Reference Osc Thread

There is a thread that produces a `reference oscillator`.  Picture
this as a packet source from a remote device, maybe an ESP32 with a
GPS module that produces UDP packets over wifi on every 1PPS pulse.
However, 1PPS is far too slow for simulation purposes, so this
produces packets at 100 Hz.  It also adds up to 1 millisecond of
jitter on every packet.


## Local Osc Thread

There is a thread representing a local oscillator. This is currently
missing the divider, but picture a local 10MHz oscillator that you
want to be disciplined to a high degree of accuracy with the 1PPS.  As
with the Reference oscillator thread, for simulation purposes, 1PPS is
far too slow to be interesting, so this thread also produces packets
at 100 Hz.  The frequency is tuned by the D-PLL thread by adjusting a
global variable `osc_interval`.  On a real oscillator you could write
a DAC that controls a VCO.


## D-PLL Thread

There is a thread for the actual D-PLL which receives packets from
both the ref and oscillator and uses UP and DOWN states to look for
time differences between pulse edges, adjusting `osc_interval` as
required.


## The Two Versions

`dpll_sim.cc` implements a multi-stage phase-locked loop to optimize
both lock time and output stability. The loop begins with a wide
bandwidth for rapid initial locking, then progressively advances to
stages with narrower bandwidths to minimize output jitter.

A key feature of this implementation is its stage-advancement
criteria: the algorithm will not transition to the next stage until it
observes a zero-crossing in the accumulated error. Because the system
naturally oscillates as it tracks the reference, a zero-crossing is
inevitable. Triggering the transition exactly at this zero-crossing
ensures the subsequent stage—with its tightened control
parameters—inherits a near-zero error offset. This prevents the
narrower loop from struggling to slowly correct a large initial
transient left by the previous stage.

`dpll_sim2.cc` is a different version with a different "stage"
mechanism, in which the loop bandwidth is dynamically adjusted on the
fly based on the amount of accumulated error. Where `dpll_sim.cc` has
a set of precalculated loop bandwidths, this one recalculates K_p and
K_i coefficients on the fly.  But, it uses a continuous dynamic
adjustment instead of discrete stages, gradually reducing the loop
bandwidth multiplier.  It advances in stages (enforced by the
thresholds in `calc_min_bw`) which limit the loop bandwidth until the
accumulated error has dropped below specified thresholds.


## Running

The C code for `dpll_sim.cc` is also a bash script.  Run `bash
dpll_sim.cc` and it will run `pll_coeff.py` (inserting `#define`
statements into a header file), compile the C code, then run the C
code.  I currently run it as root so it has permissions to set thread
priorities to real-time, but that isn't necessary if you just want to
see it work.

`dpll_sim2.cc` is also a bash script at the top but it doesn't run any
python to generate the the coefficients; it just compiles the file and
runs it.


## Plotting Results

The `plot.py` is matched to `dpll_sim.cc`.  The `plot2.py` is matched
to `dpll_sim2.cc`.  In each case, the plotter should be run at the
same time as the C program.  The plotter reads the `plot.dat` output
file, plotting points live as the program runs.  With this you can
observe the frequency adjustments and the phase difference
measurements, and watch them converge.


# Discussion

It is an interesting problem because you are starting with a clock
source which is stable and precise, then adding jitter in the packet
delivery. In this case, the errors will be very significant
iteration-to-iteration, but it should center around a very accurate
point. The question is, can you do enough measuring and averaging on
the recipient side to recover a highly precise version of the original
clock source.

This is a classic timekeeping problem. In fact, this exact
scenario—disciplining a local oscillator using a highly jittery
network reference—is the core engineering challenge behind the `Network
Time Protocol (NTP)` and the `Precision Time Protocol (PTP)`.


## Theory of the original project

A timing signal originating from a GPS 1-Pulse-Per-Second (1PPS)
source possesses exceptional long-term frequency stability,
effectively tied to atomic clocks. When this precise pulse triggers an
interrupt to generate a network packet, and that packet is pushed
through an operating system network stack and transmitted over Wi-Fi,
the signal degrades severely.

### The Noise Profile

The transport mechanism introduces two distinct timing errors:

  - Latency (Static Delay): The baseline time it takes for hardware
    processing, interrupt handling, and electromagnetic
    propagation. This destroys absolute accuracy (phase alignment with
    UTC) unless perfectly measured and subtracted.
  - Packet Delay Variation (PDV / Jitter): Wi-Fi utilizes Carrier
    Sense Multiple Access with Collision Avoidance (CSMA/CA). If the
    airwaves are busy, the radio buffers the packet. This introduces
    highly asymmetric, non-Gaussian jitter. The delay can spike to
    tens or hundreds of milliseconds, but it can never be shorter than
    the physical baseline latency.

### The Reconstruction Process

To strip away this network jitter and recover the underlying
precision, the receiving device must implement a Digital Phase-Locked
Loop (DPLL) or a software clock discipline algorithm.

  - Local Oscillator (LO) Dependency: The receiver must possess a
    stable local clock (like a TCXO—Temperature Compensated Crystal
    Oscillator). Because the network updates are noisy, the receiver
    must "flywheel" or maintain a steady beat on its own between valid
    measurements.
  - Phase Detection: As each Wi-Fi packet arrives, the receiver
    timestamps it using its local clock. It compares the inter-arrival
    time against the expected 1.000000-second interval to calculate a
    phase error.
  - Minimum-Delay Filtering: Because Wi-Fi jitter is strictly
    right-skewed (packets can be delayed but never early), simple
    mathematical averaging fails. Instead, algorithms use a "lucky
    packet" or minimum-filter approach. The system observes a wide
    window of packets (e.g., 64 seconds) and heavily weights the
    packets with the shortest transit times, as these represent the
    truest path with the least contention delay.
  - Narrow Loop Bandwidth: The filtered error signal is fed into a
    loop filter with a very long time constant. This intentionally
    makes the system sluggish to react. It entirely ignores rapid
    packet-to-packet swings, gently steering the frequency of the
    local oscillator to match the long-term trend of the incoming
    packets.

The fundamental reason this succeeds is that network transport does
not create or destroy packets; it only shifts them in time. The
long-term integral of the frequency error is strictly zero. By
extending the averaging window, the high-frequency Wi-Fi jitter is
completely attenuated. The output's precision becomes a hybrid: the
short-term precision (low jitter) is provided by the receiver's local
oscillator, while the long-term precision (zero wander) is anchored by
the distant GPS clock.

Because you are intentionally sacrificing accuracy (ignoring the fixed
latency offset) to isolate the stable frequency, you can successfully
reconstruct a highly precise clock over a heavily jittered Wi-Fi link.


## Theory of digital PLL (PI controller)

In an analog PLL, you have physical resistors and capacitors. The
'tank' capacitor and the charge pump current define the loop filter
corner frequency. The resistor size in series with the capacitor
determines the damping factor. Standard formulas exist for calculating
the relationship between the tank capacitor, charge pump current, and
resistor values.  The phase detector circuit produces "UP" or "DOWN"
pulses to alter the control voltage in the tank capacitor. The closer
the reference and feedback clocks, the smaller the pulses, so the more
stable the control voltage.  A "fastlock" feature is often implemented
by altering the charge pump current higher, raising the loop
bandwidth, until a "lock" detection circuit says it is locked.

In a software `Digital Phase-Locked Loop (DPLL)`, your loop filter is
implemented as a `Proportional-Integral (PI) Controller`.

Instead of an analog voltage controlling a VCO, you have software
variables adjusting the duration of a timer or a digital counter. Here
is how the analog concepts map to software:

  - Phase Error: The measured delay between your local software timer
    and the arrival of the 1PPS Wi-Fi packet.
  - Loop Filter Capacitor (`C_tank`): This becomes the Integral Gain
    (`K_i`). The integral term accumulates errors over time to
    determine the underlying frequency drift. A massive capacitor in
    hardware equals a very, very small `K_i` in software.
  - Charge Pump/Resistor: This becomes the Proportional Gain
    (`K_p`). It provides an immediate correction based on the most
    recent phase error.
  - The Translation Math: Bandwidth (`omega_n`) is roughly
    proportional to `sqrt(K_i)`. To get your desired "very low
    frequency response," you make `K_i` and `K_p` incredibly small.

Damping Factor (`zeta`) is roughly proportional to `K_p / sqrt(K_i)`.
Formulas are implemented in the python code for the first version and
in C code for the second to calculate K_p and K_i values given the
desired loop bandwidth and zeta. 

Both versions implement standard formulas for the Natural Frequency
Mapping, derived using the Bilinear Transform.

Every time a ref packet arrives, you measure the error.  Because your
`K_p` and `K_i` are tiny, the software loop barely reacts to a single
error. It just adds a tiny fraction of that error to an
accumulator. It might take 1,000 packets for the accumulator to build
up enough mathematical weight to shift your local oscillator's
frequency, effectively averaging out the jitter perfectly.


## A note on zeta

The damping parameter `zeta` configures the response of the system.

A recommended zeta for a PLL is 0.707 (`1 / sqrt(2)`).
Counterintuitively, this underdamped value actually provides a faster
lock and is widely considered the "optimal" target for a standard
second-order PLL.  To understand why engineers intentionally design a
system to ring, we have to look at the trade-offs between speed,
accuracy, and noise filtering.

Here is why an underdamped system is actually optimal for a PLL.

1. The Race to Lock: Rise Time vs. Settling Time.

When a PLL is trying to lock onto a new frequency or phase, it behaves
like a car trying to stop exactly at a stop sign.

  - Overdamped (zeta > 1): The car spots the sign from a mile away
    and gently coasts to a stop. It never passes the sign (no
    overshoot), but it takes a frustratingly long time to get
    there. This is a very slow lock time.
  - Critically Damped (zeta = 1): The car brakes perfectly, stopping
    exactly at the line as fast as possible without crossing it. While
    this seems ideal, the initial approach (the "rise time") is still
    somewhat sluggish.
  - Underdamped (zeta ~= 0.707): The car speeds toward the line, slams
    on the brakes, slides slightly past the line (overshoot), and then
    reverses quickly to stop on the line.

In a PLL, speed is highly prized. An underdamped system reacts to
input changes much faster than a critically damped one. At `zeta =
0.707`, the system overshoots its target frequency by only about
4.3%. This tiny oscillation dies out almost instantly. Therefore, the
total time it takes for the PLL to get within a tight, acceptable
margin of error (the settling time) is actually shorter at `zeta =
0.707` than it is at `zeta = 1.0`.

2. The Frequency Domain: The Butterworth Sweet Spot

PLLs are not just control systems; they are also low-pass
filters. They need to track the desired reference signal while
rejecting high-frequency noise (like reference spurs and
voltage-controlled oscillator noise).

When you look at the frequency response of a second-order system:

  - If zeta is too low (e.g., 0.3), the filter has a massive "peaking"
    effect. It will actually amplify noise at its natural frequency
    rather than filtering it out.
  - If zeta is too high (e.g., 2.0), the filter's roll-off is very
    gradual, letting high-frequency noise bleed through into the
    system.
  - At exactly zeta = 1 / sqrt(2) ~= 0.707, the system exhibits a
    maximally flat passband, known as a Butterworth response. It
    provides the sharpest possible filtering of high-frequency noise
    without causing any noise-amplifying peaks in the frequency
    domain.

3. Phase Margin and Stability.

In control theory, damping factor is directly related to Phase Margin,
which is the safety net that prevents a system from becoming
completely unstable and oscillating out of control indefinitely.

A damping factor of 0.707 correlates to a phase margin of
approximately 65 degrees. In analog circuit design, a phase margin
between 60 and 70 degrees is widely considered the "sweet spot." It
guarantees robust stability across variations in temperature,
manufacturing tolerances, and voltage, while keeping the system highly
responsive.


# A note on FPGA implementation:

See this document:

[FPGA Implementation of a Digital Phase-Locked Loop (DPLL)](https://docs.google.com/document/d/1AqE1ZTA9OA5YUef1ewwuRbSqVJFMEHu2YOxjFeLPDT0/edit?usp=sharing)

