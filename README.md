# AVRISP Emulator

A firmware implementation of the **AVRISP mkII** programmer for the **STM32F401** microcontroller. It presents itself to the host PC as a standard USB CDC serial device and speaks the **STK500v2** protocol, allowing any compatible tool (e.g. `avrdude`) to program AVR targets over ISP without any proprietary hardware.

> Tested on a **WeAct STM32F401 Blackpill** dev board programming an **Arduino Leonardo (ATmega32U4)** on a breadboard with a bidirectional level shifter.

---

![Breadboard setup](docs/demo.jpg)

---

## Features

- Full **STK500v2 protocol** implementation over **USB CDC** (no drivers needed on Linux/macOS; uses the `usbser.sys` driver on Windows)
- Programs AVR **Flash** and **EEPROM**, reads/writes **fuse bytes** and **lock bits**, reads the device **signature**
- Supports memories **larger than 128 KB** via the extended address instruction
- **Dynamic SPI clock** — the host can negotiate the SCK frequency via `PARAM_SCK_DURATION`; the firmware selects the closest achievable prescaler at run-time
- **Live target voltage sensing** — reads `VTARGET` through an on-board ADC and reports it back to the host in units of 10 mV
- Correct **RDY/BSY polling** and blind-delay fallback for chip erase and page write operations
- Error LED for quick fault indication during development

## How It Works

```
 Host PC (avrdude)
       │  USB CDC (STK500v2)
       ▼
 ┌─────────────────┐
 │  STM32F401      │   PA4 ──── RESET ────┐
 │  (this project) │   PA5 ──── SCK  ─────┤  Level   AVR target
 │                 │   PA6 ──── MISO ─────┤ Shifter  (ATmega32U4)
 │                 │   PA7 ──── MOSI ─────┘
 │                 │   PB1 ──── Vtarget sense (ADC)
 └─────────────────┘
```

1. The STM32 enumerates as a **USB CDC device**. Received bytes are placed into a **ring buffer** by the CDC RX callback.
2. The main loop polls the ring buffer and attempts to parse a complete **STK500v2 frame** (start byte `0x1B`, sequence number, 16-bit body length, token `0x0E`, body, XOR checksum).
3. A validated frame is dispatched to the appropriate command handler, which drives the **SPI1 peripheral** and the **AVR RESET line** to execute the requested ISP operation.
4. The response frame is serialised and sent back to the host via `USB_SendData`.

## Hardware

### Bill of Materials

| Component | Description |
|---|---|
| WeAct STM32F401CCU6 Blackpill | Main MCU board |
| Bidirectional logic level shifter | 3.3 V ↔ 5 V, at least 4 channels (SCK, MOSI, MISO, RESET) |
| Resistors: 3.3 kΩ and 2 kΩ | Vtarget voltage divider |
| LED + current-limiting resistor | Error indicator on PA3 |
| USB Micro-B cable | Host connection |

### Pin Mapping

| Signal | STM32 Pin | Direction | Notes |
|---|---|---|---|
| Error LED | PA3 | Output | Active-high |
| AVR RESET | PA4 | Output | Active-low; pulled high when idle |
| SPI1 SCK | PA5 | Output (AF5) | |
| SPI1 MISO | PA6 | Input (AF5) | |
| SPI1 MOSI | PA7 | Output (AF5) | |
| USB D− | PA11 | Bidirectional | |
| USB D+ | PA12 | Bidirectional | |
| Vtarget sense | PB1 | Analog input | ADC1 channel 9 |

### Vtarget Voltage Divider

The target supply voltage is measured via a resistive divider that scales the rail down to the ADC's 3.3 V input range:

$$V_{out} = V_{target} \times \frac{R_2}{R_1 + R_2} = V_{target} \times \frac{2000}{5300}$$

Connect the divider between the target VCC, PB1, and GND.

### Clock Configuration

The firmware uses the **internal HSI oscillator (16 MHz)** as SYSCLK for simplicity and robustness, while the PLL (sourced from the 25 MHz HSE crystal on the Blackpill) is enabled solely to produce the mandatory **48 MHz USB clock** via the PLLQ output:

$$f_{USB} = \frac{f_{HSE} \times N}{M \times Q} = \frac{25\,\text{MHz} \times 144}{15 \times 5} = 48\,\text{MHz}$$

## Building

### Prerequisites

- `arm-none-eabi-gcc` toolchain (≥ 10)
- `cmake` ≥ 3.22
- `ninja`

```bash
# Configure (Debug)
cmake --preset Debug

# Build
cmake --build --preset Debug
```

The output ELF and map file are placed in `build/Debug/`.

A `Release` preset is also available (`-Os`, no debug info):

```bash
cmake --preset Release && cmake --build --preset Release
```

### Flashing

Flash with OpenOCD and an ST-Link (configuration provided in `openocd.cfg`):

```bash
openocd -f openocd.cfg \
        -c "program build/Debug/avrisp_emulator.elf verify reset exit"
```

Or use any SWD-capable probe supported by your tooling.

## Usage

Once flashed, connect the STM32 to the host via USB. A CDC serial port will appear (e.g. `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem*` on macOS, `COMx` on Windows).

Use `avrdude` with the `stk500v2` programmer type:

```bash
# Read fuses from an ATmega32U4
avrdude -c stk500v2 -p atmega32u4 -P /dev/ttyACM0 -v

# Write a firmware image
avrdude -c stk500v2 -p atmega32u4 -P /dev/ttyACM0 -U flash:w:firmware.hex:i
```

> **Level shifter required** when programming 5 V AVR targets from a 3.3 V STM32. Omitting it risks damaging the MCU.

## Project Structure

```
├── CMakeLists.txt              # Build definition
├── CMakePresets.json           # Debug / Release presets
├── gcc-arm-none-eabi.cmake     # Toolchain file
├── STM32F401XX_FLASH.ld        # Linker script
├── startup_stm32f401xc.s       # Cortex-M4 startup / vector table
├── Include/                    # Project header files
│   ├── stk500v2.h              # Protocol constants, types, command IDs
│   ├── usb.h                   # USB handle and command handler interface
│   ├── spi.h                   # SPI driver interface
│   ├── adc.h                   # ADC (Vtarget) driver interface
│   ├── gpio_defs.h             # Pin assignments
│   ├── app_state.h             # Global peripheral handle struct
│   └── ...
├── Src/                        # Application source files
│   ├── main.c                  # Initialisation and entry point
│   ├── stk500v2.c              # STK500v2 protocol parser and handler
│   ├── usb.c                   # USB CDC init and message loop
│   ├── spi.c                   # SPI master driver
│   ├── adc.c                   # ADC driver
│   ├── power.c                 # System clock configuration
│   ├── error.c                 # Error LED and fatal halt
│   └── msp/                    # HAL MSP (low-level peripheral init callbacks)
├── lib/
│   ├── CMSIS/                  # ARM CMSIS headers
│   ├── HAL/                    # STM32F4 HAL driver
│   ├── USB/                    # STM32 USB device library (CDC class)
│   └── ring_buffer/            # Lightweight ring buffer
└── docs/                       # Reference documents and images
```

## References

- [STK500v2 Communication Protocol Specification](docs/doc2591.pdf)
- [STM32F401 Reference Manual](docs/reference_manual.pdf)
- [STM32F4 HAL Driver User Manual](docs/HAL.pdf)
- [ARM Cortex-M4 Technical Reference Manual](docs/CM4.pdf)
- [ATmega32U4 Datasheet](docs/datasheet.pdf)
