
# Digital Phase Locked Loop Simulator

This is a software simulation of a `Digital Phase Locked Loop`.

The exact scenario which triggered this project was one discovered
online, in which the author attempted to discipline a 1PPS oscillator
by using NTP over wifi. He encountered significant jitter but was
trying to use a form of proportional integrator to average out the
jitter and recover the original high precision timing.  He never got
it to work, but I believed it was due to errors in configuration of
the loop parameters. He was experimenting in the dark, just trying
stuff, rather than understanding the math behind the scenario.

This tool simulates such an environment, but using the math correctly.

## Reference Osc Thread

There is a thread that produces a `reference oscillator`.  Picture
this as a packet source from a remote device, maybe an ESP32 with a
GPS module that produces UDP packets over wifi on every 1PPS pulse.
This simulation adds up to 5 milliseconds of jitter on every packet.

## Local Osc Thread

There is a thread representing a local oscillator. This is currently
missing the divider but picture a local 10MHz oscillator that you want
to be disciplined to a high degree of accuracy with the 1PPS.  The
local oscillator thread produces pulses also at 1PPS; if we had the
divider, it would just count to ten million or something and deliver a
packet only on the 1 second intervals.  The frequency of this
oscillator is tunable, in the simulation by adjusting a global
variable `osc_interval`.  On a real oscillator you would write a DAC
that controls a VCO.

## D-PLL Thread

There is a thread for the actual D-PLL which receives packets from
both the ref and oscillator and uses UP and DOWN states to look for
time differences between pulse edges, adjusting `osc_interval` as
required.

## Running

The C code is also a bash script.  Run `bash dpll_sim.cc` and it will
run `pll_coeff.py` (inserting `#define` statements into a header
file), compile the C code, then run the C code.  I currently run it as
root so it has permissions to set thread priorities to real-time, but
that isn't necessary if you just want to see it work.

## Plotting Results

The `plot.py` runs at the same time and reads the `plot.dat` output file,
plotting points live as the program runs.  With this you can observe the
frequency adjustments and the phase difference measurements, and watch
them converge.

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

### A hardware solution

Suppose you were building a pure hardware PLL to implement this.

If the source is perfectly stable but the transport channel is
incredibly noisy, and you do not care about lock time, you want a very
narrow loop bandwidth.

1. Adjusting `I_cp` and `C_tank` for Narrow Bandwidth.

In a charge-pump PLL, the natural frequency (which dictates the loop
bandwidth, (`omega_n`) is proportional to `sqrt(I_cp / C_tank)`. To
narrow the bandwidth, you would minimize the charge pump current
(`I_cp`) and maximize the size of your loop filter capacitor
(`C_tank`).

By doing this, the PLL acts as an aggressive Low-Pass Filter for
Phase. It becomes virtually blind to the high-frequency cycle-to-cycle
jitter (the variable packet timing) and only responds to the
long-term, heavily averaged DC phase difference.

Caveat: Because the PLL is largely ignoring the reference signal in
the short term, your local `Voltage Controlled Oscillator (VCO)` must
have excellent short-term stability (low phase noise/drift) so it
doesn't drift away while the loop filter is "thinking."

2. Should you use a `zeta` other than 0.707?

Yes. In extreme high-jitter scenarios, you often want to heavily
over-damp the system (e.g., `zeta = 1.5 to 5.0`).

While `zeta = 0.707` is optimal for quick settling, it exhibits a
phenomenon called Jitter Peaking. At frequencies near the loop
bandwidth, a critically damped or slightly underdamped PLL actually
has a gain slightly greater than 1. This means it amplifies jitter at
that specific resonant frequency.

Because you have massive transport jitter and zero care for lock time,
you want to completely eliminate jitter peaking. Increasing `zeta`
flattens the frequency response, ensuring that the jitter is strictly
attenuated and never amplified. It will take a glacial amount of time
to lock, but the lock will be rock solid.


### A software solution

In an analog PLL, you have physical resistors and capacitors. In a
software `Digital Phase-Locked Loop (DPLL)`, your loop filter is
usually implemented as a `Proportional-Integral (PI) Controller`.

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

Damping Factor (`zeta`) is roughly proportional to `K_p /
sqrt(K_i)`. To get your heavily overdamped, no-jitter-peaking response
(`zeta > 1`), you ensure that your Proportional gain (`K_p`), while
small, is mathematically much larger than your Integral gain (`K_i`).

### How it works in practice (1PPS over Wi-Fi)

Every time a packet arrives, you measure the error (e.g., +15ms,
-40ms, +5ms). Because your `K_p` and `K_i` are tiny, the software loop
barely reacts to a single +15ms error. It just adds a tiny fraction of
that error to an accumulator. It might take 10,000 packets (over 2.5
hours at 1PPS) for the accumulator to build up enough mathematical
weight to shift your local oscillator's frequency, effectively
averaging out all the Wi-Fi jitter perfectly.



# Configuring DPLL parameters

The python function `calculate_pll_coeffs` in `pll_coeff.py`
implements standard formulas for the `Natural Frequency Mapping`,
derived using `Impulse Invariant Mapping`.


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
