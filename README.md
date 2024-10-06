# Pico SD Bootloader

A Stage 3 Bootloader for the Raspberry Pi Pico family that enables booting application binaries from an SD card.

## Overview

This project consists of two main components:

### Bootloader

The bootloader is a standard application that initializes the Raspberry Pi Pico. It checks for a valid application at a specified offset in the flash memory and jumps to it if found.
If no valid application is present, it attempts to load an `firmware.bin` file from an SD card connected to the Pico. The SD card interface is configured in `hw_config.c`. It uses the [no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico?tab=readme-ov-file#cc-library-for-sd-cards-on-the-pico) library.

### Application

The application provided is a simple blink example.
It is linked using a custom linker script that offsets the flash region by 128KB to accommodate the bootloader (which is approximately 100KB in size) and ignores the .boot2 section of the linker script. This application demonstrates how to build and link your own programs to work with the bootloader.

## Installation Instructions

### Prerequisites

- **Hardware**

  - Raspberry Pi Pico
  - SPI Micro SD card module (mine is a 5V and needs an external power supply, be careful, wasted 4 hours of my life trying to power it with 3.3VDC)
  - FAT32 formatted MicroSD card (SDHC or SDXC, tested with 16Gb)
  - Good quality breadboard and shortest possible jumper wires

- **Software**
  - [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) installed and configured
  - CMake
  - GNU Make
  - ARM GCC toolchain

### Setting Up the Environment

1. **Clone the Repository**

   ```bash
   git clone https://github.com/julienfdev/pico-sd-bootloader.git
   cd pico-sd-bootloader
   ```

2. **Initialize the submodules**

   The FatFS library lives as a submodule within the repo, it must be initialized

   ```bash
   git submodule update --init
   ```

3. **Set Up the Pico SDK Path**

   Ensure that the `PICO_SDK_PATH` environment variable points to your local copy of the Pico SDK.

   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

### Building and Flashing the Bootloader


1. **Configuring the bootloader code**

   Please adjust the lib/sd/hw_config.c as you see fit (it should be properly configured for you, if you follow the "Wiring the hardware" paragraph).
   
   Decide on whether or not you want to comment out the DEBUG_STDIO_DELAY variable. When defined, it gives you a 5 second delay to connect to the USB 
   Serial Monitor before flashing the firmware or starting the application.

3. **Build the Bootloader**

   ```bash
   cd bootloader
   mkdir build
   cd build
   cmake ..
   make
   ```

4. **Flash the Bootloader to the Pico**

   - Connect the Pico to your computer while holding the **BOOTSEL** button to enter USB mass storage mode.
   - Copy the generated `bootloader.uf2` file from the `bootloader/build` directory to the Pico's storage.

### Building the Application

1. **Build the Application**

   ```bash
   cd ../../firmware
   mkdir build
   cd build
   cmake ..
   make
   ```

2. **Prepare the Application Binary**

   - The compiled binary will be located at `firmware/build/firmware.bin`.

3. **Copy to SD Card**

   - Copy `firmware.bin` to the root directory of your FAT32-formatted SD card.

### Wiring the Hardware

Connect the SD card module to the Raspberry Pi Pico as follows:

| SD Card Module | Raspberry Pi Pico | External 5V Power Supply |
| -------------- | ----------------- | ------------------------ |
| VCC            | NC                | VCC                      |
| GND            | GND               | GND                      |
| SCK            | GPIO 2 (Pin 4)    | NC                       |
| MOSI           | GPIO 3 (Pin 5)    | NC                       |
| MISO           | GPIO 4 (Pin 6)    | NC                       |
| CS             | NC                | GND                      |

You can add a switch on BOOTLOAD_PIN (14 by default) to force the bootloader into upload mode, even if a valid application is detected.

![IMG_2788](https://github.com/user-attachments/assets/f4f98b05-5b99-40da-ad80-ee10398b4232)

NB: while i'm using a Pico W with external LEDs connected, the project is configured for a classic Pi Pico blinking the inboard LED, you don't need the two top grey wires, resistors and LEDs

### Running the Application

1. **Insert the SD Card**

   - Insert the SD card into the SD card module connected to the Pico.

2. **Power Cycle the Pico**

   - Disconnect and reconnect the Pico to power it up.
   - The bootloader will attempt to load `firmware.bin` from the SD card if no valid application is found in flash memory.

3. **Observe the Blink Example**

   - If successful, the Pico's onboard LED should start blinking, demonstrating that the application has been loaded from the SD card.

## Known Limitations

- **No Checksum Verification**
  The bootloader does not perform checksum verification on the application binary loaded from the SD card. This may result in undefined behavior if the binary is corrupted or improperly formatted.
  The original code by V. Hunter Adams was based on UART transmission of an HEX file, allowing for line-by-line checksum verification.

  However, the parser wasn't meant to handle firmwares >32Kb and failed to handle the second iteration of the 0x04 code. Ignoring the code wasn't enough and resulted in corrupted application binary.

## Acknowledgements

This project is based on the work of V. Hunter Adams. Special thanks for the foundational concepts and inspiration.

- [V. Hunter Adams' Website](https://vanhunteradams.com/Pico/Bootloader/Bootloader.html) <!-- Please replace '#' with the actual URL -->

## License

This project is licensed under the MIT License
