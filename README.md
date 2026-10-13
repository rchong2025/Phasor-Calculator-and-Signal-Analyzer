# EFM8LB1 Phasor Calculator & Signal Analyzer 
A microcontroller-based digital signal analyzer and phasor calculator built around the Silicon Labs EFM8LB1 microcontroller. This project samples dual analog input channels, computes real-time RMS voltage, frequency, and phase relationships, and provides an interactive interface for performing complex phasor arithmetic using hardware pushbuttons.

## Features:

### High-Speed Analog Sampling: 
- Configured for a 72 MHz system clock and a 14-bit ADC mode to maximize measurement accuracy.
### Real-Time Signal Analysis: 
- Measures and calculates reference and signal RMS amplitudes, signal frequency and phase difference between channels using precision hardware timers (Timer2 and Timer3).
### Custom 8051-Optimized Math Library: 
- Implements lightweight floating-point approximations for sin, cos, sqrt, atan, and atan2 designed for efficient execution on 8051 architectures.
### Interactive Phasor Calculator: 
- Features dedicated pushbuttons to perform vector/phasor operations directly on the device:
    - Add / Subtract: Converts polar coordinates to rectangular form, performs vector addition/subtraction, and converts results back to magnitude and phase.
    - Multiply: Multiplies magnitudes and adds phase angles.
    - Save: Captures current RMS and phase metrics into selectable bus registers.
### Dual-Interface Output: 
- Real-time telemetry streamed via UART to a terminal emulator (e.g., PuTTY) alongside a live 16x2 character LCD status display.

## Hardware implimentations: 
- Microcontroller: Silicon Labs EFM8LB1.
- Display: Standard 16x2 character LCD (operating in 4-bit mode).
- Pushbuttons for arithmetic operations (ADD, SUB, MUL, SAVE, and DISPLAY) mapped to designated GPIO pins.
- Communication: USB-to-UART bridge for debug logging and telemetry.
