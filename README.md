# Modular Audio Processor (Production Release)

## Overview
A real-time audio effects processor built on the Adafruit Feather RP2040. It processes I2S digital audio through various filters (Low-pass, High-pass, Gain, Quantize) and provides a live waveform/FFT visualization on an SSD1306 OLED display.

## Architecture
The system utilizes a multi-core architecture to ensure real-time audio processing performance.

- **Core 0 (Real-time Audio):**
    - Manages I2S Rx/Tx PIO state machines and DMA channels (`I2S.cpp`, `TxPingPong.cpp`).
    - Processes audio samples using floating-point filters (`filters.cpp`).
    - Maintains cyclic buffers for filter history (`CyclicBuffer_t`).
- **Core 1 (UI & Control):**
    - Renders the OLED display (`oled.cpp`), switching between Waveform and FFT views.
    - Handles user input via rotary encoders and push buttons (`RotaryEncoder.hpp`, `PushButton.hpp`).
    - Manages non-volatile storage of settings in flash memory.
- **Inter-Core Communication:**
    - A FIFO queue (`sharedQueue`) passes downsampled audio data from Core 0 to Core 1 for visualization without locking.

## Features
- **Audio Effects:**
    - **SRC:** Pass-through
    - **LPF/HPF:** First-order IIR filters with adjustable cutoff (`alpha`)
    - **GAIN:** Quadratic gain scaling
    - **QTZ:** Bit-crusher/Quantizer effect
- **Visualization:**
    - Real-time Oscilloscope
    - Real-time FFT Spectrum Visualizer (256 bins)
- **Persistence:** Saves current Mode and Alpha value to flash memory. It automatically restores on next power-up.

## Hardware Components
- Microcontroller: Adafruit Feather RP2040
- Display: SSD1306 OLED (128x64)
- Audio Codec: PCM5102 DAC Decoder Module and Mini Loudspeaker (3 Watt, 4 Ohm)
- Input: 2x CYT1100 Rotary Encoders with Push Switch

## Pinout
| Component       | Pin Function | GPIO Pin |
|-----------------|--------------|----------|
| **OLED Display**| SCK          | 10       |
|                 | MOSI (TX)    | 11       |
|                 | RST          | 28       |
|                 | DC           | 29       |
|                 | CS           | 24       |
| **I2S Audio**   | Tx Data      | 0        |
|                 | Tx BCLK      | 2        |
|                 | Tx WS (LRCLK)| 3        |
|                 | Rx Data      | 7        |
|                 | Rx BCLK      | 8        |
|                 | Rx WS (LRCLK)| 9        |
| **Controls**    | Mode Enc A   | 12       |
|                 | Mode Enc B   | 13       |
|                 | Mode SW      | 25       |
|                 | Alpha Enc A  | 1        |
|                 | Alpha Enc B  | 6        |
|                 | Alpha SW     | 20       |

## Build Instructions
1. Install the Raspberry Pi Pico SDK
2. Clone this repository
3. Create a `build` directory: `mkdir build && cd build`
4. Run CMake: `cmake ..`
5. Compile: `make`
6. Flash the `.uf2` file to the RP2040

## Usage
- **Mode Selection:** Turn the *Left* encoder to cycle between effects (SRC, LPF, HPF, GAIN, QTZ, FFT). Press the *Left* encoder button to reset to SRC mode.
- **Parameter Adjustment:** Turn the *Right* encoder to adjust the filter parameter (`alpha`). Press the *Right* encoder button to reset the parameter to `1.00` (or max).
- **Visualization:** The display will automatically show the waveform or FFT based on the selected mode.
- **Saving:** Settings are saved automatically after the last change.

## Known Bugs
- When continuously changing the alpha value when there is an incoming signal, both the display and audio output pause very briefly. This is because saving the flash memory operates within an ISR, which briefly stops the rest of the program to complete the flash write. We were not able to implement a solution to this as the ISR is needed to prevent reading and writing at the same time, which caused the system to crash during testing.

---

# Modular Audio Processor (Release Candidate)

## Overview
In the Release Candidate version of the Modular Audio Processor, the system consists of a dedicated input and one or more functions that connect via POGO pins. The system supports real-time DSP filtering and live FFT visualization, with all settings capable of being stored in flash memory. The Release Candidate represents a mostly polished build, as all planned features are integrated, the casings are complete, and usability has been refined based on feedback from our beta testing phase.

## Completed Work

### Hardware and Physical Design
- Designed and constructed dedicated input and function module casings (3D-printed and assembled).
- Input Module now uses a 3.5mm headphone jack for analog audio input instead of the DAD board's Waveform Generator.
- Function Modules output processed audio through a dedicated DAC with a 3.5mm headphone jack.
- System power is supplied using a USB-C breakout board (no direct connection to RP2040 USB-C port).
- User controls now use a rotary encoder instead of a potentiometer for intuitive control. Turning it adjusts the filter parameter alpha and pressing it changes the current DSP mode.
- Another button was added to allow the user to save these settings to flash memory, allowing the configuration to persist even after the system loses power.

### Intermodular Communication
- Full I2S-based communication using the RP2040 PIO for stereo-aligned, low-latency audio at 44.1 kHz
- We're still using dual-core optimization, with Core 0 handling the real-time audio pipeline (I2S RX -> DSP -> I2S TX) and Core 1 handling the OLED display logic.
- Added clock-forward PIO program to maintain clean BCLK/WS synchronization across chained modules.

### DSP Features
- The system now uses fixed-point arithmetic throughout for low latency and highly precise processing.
- Downsampled waveform/FFT data streamed to the display without impacting audio performance.

## Project Architecture

### Module Types
- **Input Module**: Reads analog audio from the 3.5mm headphone jack via the RP2040 ADC, performs fixed-point conversion, and transmits stereo I2S audio at 44.1 kHz frequency over POGO pins.
- **Function Module**: Receives I2S audio via POGO pins, applies the selected filter or FFT, and forwards the processed audio via I2S TX. It also handles user input (rotary encoder + mode button), flash persistence, and OLED display.

### Core Allocation
- **Core 0** (audio/DSP): I2S RX -> selectable filter or FFT -> I2S Tx. Uses DMA ping-pong buffers for zero-copy, real-time operation. It includes downsampling for display data.
- **Core 1** (OLED Display): Drives the 128 by 64 SSD1306 OLED display, renders either a live oscilloscope-style waveform display or FFT bar graph, and displays current mode and alpha value.

### State & Persistence
- Mode and alpha are stored in the last flash sector using a magic-byte header.
- `loadMode()` and `saveMode()` run at boot and on every user change.
- Flash writes are performed safely with `flash_safe_execute` to avoid XIP conflicts.

### Inter-Module Communication
- I2S bus (BCLK, WS, SD) carried over POGO pins.
- Dedicated PIO program forwards the clock signals to maintain synchronization in long chains.
- Power and ground are also passed through the POGO connectors.

## Known Bugs
- If the user partially disconnects the modules in such a manner that the POGO pins transmitting the I2S signals are detached but VCC and GND are stil connected, once the user fully reconnects the modules, it messes up the audio signal. This is caused by the PIO using autopull. However, this would be something that the user would need to delibrately do and isn't an issue that would usually come up in normal use.
- There are rare DMA underflow events that print a debug message to UART during extreme buffer starvation (though this is never audible in normal use and is automatically recovered by inserting default data).
- In this version, there are no major bugs, crashes, or loss of functionality. The edge cases (power-cycle, rapidly changing modes, alpha = 0, and buffer overflow/underflow) are handled properly.

---

# Modular Audio Processor (Beta Build)

## Completed Work
During the beta building phase, we primarily focused on transitioning from our breadboard prototype to a robust, integrated hardware system and improving the intermodular communication.

### Hardware and Physical Design
- Designed and implemented the first iteration of 3D-printed casings and moved away from prototyping breadboards.
- Replaced breadboard wiring with magnetic pogo pins for connection between modules
- Most internal components have been successfully fitted into their respective module casings.

### Firmware and Intermodular Communication
- Switched from SPI to I2S protocol using the RP2040's PIO to ensure stereo alignment and better audio quality.
- Switched from Arduino to the C++ SDK for coding to have better control over precision timing.
- Modules now have a persistent state in which they store the most recent filter settings and parameters in memory and retaining them even after being disconnected.
- Implemented a dual-core optimization strategy in which Core 0 handles I2S and DSP operations and Core 1 handles the UI and OLED display to prevent high latency.

### DSP Features
- Implemented special effect modes (Reverb, Gain, and Distortion) with adjustable parameters.

## Project Architecture

### Core Allocation
- The system is built on the Adafruit Feather RP2040 dual-core architecture.
- Core 0 is responsible for starting I2S DMA, reading 16-bit samples, applying DSP math (filtering/effects), and writing the output to the I2S bus.
- Core 1 is responsible for managing the user interface, including reading the adjustable knob value, plotting the waveforms/FFT, and refreshing the OLED display.

### State Logic
- The modules operate on a state-machine logic.
1. Upon powering up, the module searches for a Word Select and Bit Clock signal for syncing.
2. Once a clock match is found, the module enters a processing loop (Read -> Process -> Write)
3. If a signal is lost, the system enters an error state or returns to syncing until a connection is restored.

## Known Bugs and Limitations
- There are occassional connectivity issues with the pogo pins. They sometimes require manual adjustment to maintain a signal.
- The physical housing for user controls (knobs/buttons) have not yet been robustly designed.
- Dedicated input and output modules have not yet been implemented. Currently, power and analog signals are supplied via manual wiring.
- While the 3D-printed casing is structurally secure, the internal components require further mounting to prevent movement inside the casing.

---

# Modular Audio Processor (Alpha Build)

## Architecture

Mostly follows the prototype build:

- Targets Adafruit Feather RP2040
- Application state stored to FLASH
- SSD1306 display still in-use
- Difference from prototype: switch from SPI to I2S for intermodular communication
- Difference from prototype: ADC input/DAC output in process of being separated

## Completed Work

- Persistent state by storing filter parameter(s)
- External interface via display to user & interactable buttons/knobs
- Internal systems (filtering)

## Bugs & Incompleteness

- Intermodular communication is currently incomplete due to migration to I2C
- No 3d printed casing model so far

---

# Modular Audio Processor (Prototype Build)

The prototype build targets the Adafruit Feather RP2040 (8MB external QSPI flash) and focuses on real-world hardware integration with persistent storage and safe concurrent ADC operation during flash programming.

## Completed Work

### Persistent Storage (Flash-backed):

- Implemented a single-sector, page-aligned flash storage module.
- Stores application state (effect type, alpha parameter, write counter) in external QSPI flash at a safe offset (4MB).
- Uses a write counter to track saves and detect stale data on startup.
- At initialization (`persistent_init()`), the module reads the stored record from flash via XIP (execute-in-place) and stores to an in-memory cache.
- On save (`persistent_save()`), the new state is written to flash with automatic counter increment and post-write verification.

### OLED Display

The display utilizes custom drivers to interface with the SSD1306 display over SPI. The pins are pre-configured to use the top-left most pins on the Feather RP2040.

### ADC Signal Input

The ADC initializes pins A0-A3 as ADC channels.
- Allows for enabling/disabling the of ADC, alog with configuration.
- Reads from the active ADC channel, and updates that channel to show it has new data.
- Allows for interrupts, which helps with consistend sampling rates if desired.
- Lets the ADC read once or run at a consistend sampling rate, writing to the ADC FIFO.

## Known Bugs and Issues

- Wanted to implement round robin wear spreading for the persistent memory to limit how much each section of flash is written to. However, to do this we would have had to use four 4096-byte sectors for only a small persistent state memory. This would have been too much memory for so little, so had to be abandoned for now.
- ADC interrupts may interfere with peristent storage.

## Building

- Download the Pico-SDK and ensure the environment variable `PICO_SDK_PATH` on your system is set to it's location.
- Ensure the `arm-none-eabi` compile toolchain is installed.
- From root, run:
```
mkdir build
cd build
cmake ..
make
```
- This should create the .uf2 file in `/build` to install on the Feather RP2040

---

# Modular Audio Processor (Pre-Alpha Build)

For this pre-alpha build, we decided to use a Wokwi simulation of a Pi Pico (which also uses the RP2040 chip). This way, we can test the feasibility of our design before we invest resources into physically constructing a prototype. In summary, this pre-alpha build reads an analog signal from a potentiometer that simulates audio input, applies a configurable digital filter, and displays both the original and filtered waveforms on two separate OLED displays.

## Completed Work

Core Hardware Initialization: The major peripherals for this system are configured.
- ADC: Two channels (GP26 for signal, GP27 for filter control).
- GPIO: Two buttons (GP28 for *alpha* parameter adjustment, GP22 for save).
- SPI: spi0 is initialized and transmits the raw ADC signal.
- I2C: Two separate I2C buses (i2c0 and i2c1) are set up.

### Dual OLED Driver:

- A custom driver for the SH1107 OLED display is complete.
- It successfully manages two separate displays on two different I2C ports, each with its own framebuffer.

### Waveform Visualization:

- The draw_waveform function correctly renders a 128-sample circular buffer to the OLEDs, which creates a scrolling effect (similar to an oscilloscope).
- Display labels ("OG", "LPF") and simple borders are also drawn.

### Filter Implementation:

- A first-order IIR Low-Pass Filter is implemented.
- A High-Pass Filter is also implemented but not currently used.
- The main application uses a function pointer to select the active filter, which makes the system expandable.

### Dynamic Filter Control:

- The filter's *alpha* coefficient, which controls the cutoff frequency, can be adjusted in real-time.
- Holding button 1 reads ADC channel 1 and maps its value to an *alpha* between 0.05 and 0.95.

### Persistent State (Emulated):

- A module for saving and loading the *alpha* parameter is implemented.
- Pressing button 2 saves the current *alpha* value.
- The value is loaded on startup (this is currently an in-memory emulation and not an actual non-volatile flash storage).

### Error Detection Module (SignalErrorDetection.c):

- Functions for Checksum-8 and CRC-8 encoding and decoding have been written.
- Unit test functions (testChecksum8, testCRC8) are implemented and run at startup to verify the logic.

## Project Architecture

### Main Application (main.c)

1.  Initialization:
- Initializes the hardware peripherals (GPIO, SPI, ADC, I2C)
- Calls `persistent_init()` and `persistent_load()` in order to get the last saved *alpha* value.
- Initializes the two OLED displays
- Fills signal buffers with an initial ADC reading to prevent noise.
- Runs the `testChecksum8()` and `testCRC8()` self-tests.
- Sets the `cur_filter` function pointer to the low-pass filter configuration.
2. Main Loop:
- Uses input polling to check the state of the two buttons (*alpha* adjustment and save)
- *Alpha adjustment* (Button GP28): When pressed, it pauses the loop and `adc_read()` is called on channel 1 to update the *alpha* variable.
- Save (Button GP22): When pressed, the current *alpha* value is saved to the `PersistentState` struct and `persistent_save()` is called.
- During signal processing, the input signal (`og_signal`) is read from ADC channel 0. The input signal is transmitted via SPI. Then, the signal is stored in the `input_buffer`. Then, the `cur_filter` function, which points to the `low_pass` configuration, is called to calculate the filtered sample. This result is stored in the `filter_buffer` and the circular buffer index is incremented.
- During rendering, `oled_fill` clears both the OLED framebuffers. The `input_buffer` is drawn to OLED display 0 (OG) and the `filter_buffer` is drawn to OLED display 1 (LPF). Both of these framebuffers are pushed to the displays using `oled_update`.
- A delay, `sleep_ms(50)` is added to configure the sample and refresh rate of the main loop.

### Core Modules

- `oled.h` and `oled.c`: Manages both the SH1107 OLED displays. It takes an ID number, 1 or 0, to choose the appropriate I2C bus and corresponding framebuffer for all the drawing operations.
- `filters.h` and `filters.c`: Contains the mathematical logic for the digital filters. They operate on the original and filtered signal buffers in order to compute the next outputsample.
- `persistent_state.h` and `persistent_state.c`: This module emulates non-volatile storage. The `static PersistentState` variable holds the saved data. When we begin constructing a physical prototype of our project, this module will be responsible for writing to the flash memory.
- `SignalErrorDetection.h` and `SignalErrorDetection.c`: This is a utility module for data integrity checks. It is not being used by the main signal processing loop.

## Known Bugs and Issues

- Our `high_pass` filter implementation has a weird behavior and is not working as expected. Even though our current implementation is based on a standard discrete-time transfer function, it is not performing a true high-pass filter on the input signal and needs further investigation.
- The button used for the save state doesn't have any debouncing logic. This also applies to the *alpha* adjustment button and the "next filter" button; holding down the latter could cause the system to swap between filters repeatedly instead of registering it as a single press.
- There is an issue with our display in that a waveform wraps around (this means that after a waveform shape leaves from the left side, it comes back to the right side).
- Right now, the `persistent_state` module does not save to flash mmory, as any saved `alpha` value is lost when the Wokwi simulation is reset. However, this can be implemented properly when we use the Pico's SDK flash memory API during the physical construction of our prototype.
- Since Wokwi only allows one MCU in a simulation, we were not able to formally verify the functionality of receiving data via SPI. While the RP2040 does have two SPI buses, using both of them to test transmission and receiving caused blocking, which created a lot of lag in the Wokwi simulation. Thus, we could not simulate the daisy-chained SPI communication between modules. However, we were able to verify the functionality of the SPI transmission using a logic analyzer to observe the SCK, CS, and TX signals. Thus, we do not have reason to believe that another RP2040 would have difficulties receiving the data via SPI.
