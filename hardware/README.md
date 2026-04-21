# magicTool KiCad hardware

This directory contains a generated KiCad 10 project for a small Raspberry Pi Pico 2 W carrier board matching the firmware pin map.

## Electrical interface

- `A1`: Raspberry Pi Pico 2 W module, using KiCad's Pico W SMD hand-solder footprint.
- `J1`: 8-pin 2.54 mm debug I/O header.
- `J1.1..J1.4`: `OUT0..OUT3`, mapped to Pico GPIO `2..5`.
- `J1.5..J1.6`: `IN0..IN1`, mapped directly to Pico GPIO `6..7`.
- `J1.7`: Pico `+3V3` output for low-current external references only.
- `J1.8`: `GND`.
- Inputs use the firmware's internal pulldowns.
- `J2`: RUN-to-GND reset header.

The board assumes USB power and USB data come from the Pico module's onboard connector.

## Regeneration

Run this from the repository root:

```bash
python3 hardware/generate_magictool_kicad.py
```

Then refill zones and run KiCad DRC:

```bash
kicad-cli pcb drc --refill-zones --save-board hardware/magictool.kicad_pcb
```
