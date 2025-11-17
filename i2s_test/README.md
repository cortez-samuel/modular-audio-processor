# Modular Audio Processor (Prototype Build)
 Text here

## OLED Display
The display utilizes custom drivers to interface with the SSD1306 display over SPI. The pins are pre-configured to use the top-left most pins on the Feather RP2040 (A0, A1, A2, A3, 24). 

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

## Resources
- This project uses code from the rp2040_i2s_example repository under the GNU General Public License.
``
https://github.com/malacalypse/rp2040_i2s_example
``

<br><br>
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