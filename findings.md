# STC8G1K08 Monitor Findings

- Latest schematic `app/doc/V853(26.07.09).pdf` maps V853 PH15 to U14 Pin1/P5.4/INT2 (`CPU_CHECK`). U14 Pin3/P5.5 is the AP-RESET/Q23 side.
- Board measurement confirms a 100 ms high / 100 ms low PH15 waveform and an identical waveform at U14 Pin1/P5.4.
- The monitor firmware therefore counts INT2 falling edges. A 200 ms period yields about five edges per second.
- P5.5 has previously measured around 1.6-2.2 V through the Q23/reset network. That voltage is a board bias condition, not a target level for this firmware. High impedance must be confirmed by no change/no pulse at P5.5 and AP-RESET.
- The monitor project has no P5.5 assignment, no Q23 access, and no `WDT_CONTR` access.
