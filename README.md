# nrf-theseus
Base repository for the nRF Theseus project — a bare-metal FreeRTOS port for
Nordic nRF SoCs (Arm Cortex-M33), built and managed with west. The nRF54L15
and nRF9151 are currently supported targets.

--------------------------------------------------

<br>

## Prerequisites
Install these before setting up the workspace:

- **west**: tool for setup and building: `pip install west`.
- **arm-none-eabi GCC**: the toolchain for the Cortex-M33:
  `sudo apt install gcc-arm-none-eabi`.
- **Go**: the NimBLE build compiles Apache Mynewt's `newt` tool from source:
  `sudo apt install golang-go`.
- **nrfutil**: required by `west flash` and CMake flash target to program the device.
  [Install from Nordic](https://www.nordicsemi.com/Products/Development-tools/nRF-Util).


## Setup workspace
This repo is a west manifest repository, so it lives inside a workspace
directory. Create the workspace, clone the repo into it, then let west fetch
the dependencies:

```shell
mkdir ~/my_ws
cd ~/my_ws
git clone https://github.com/sondrep/nrf-theseus.git
west init -l nrf-theseus
west update
```

## Building and flashing a sample with west
```shell
cd ~/my_ws/nrf-theseus
west build -s SAMPLE_NAME -b TARGET_BOARD
west flash
```

- `SAMPLE_NAME` — a directory under `samples/`: `blinky`, `bluetooth`,
  `hello_theseus`, `modem`, `nfc`, or `zigbee`.
- `TARGET_BOARD` — a target from `cmake/targets/`, without the `.cmake` suffix:
  `nrf54l15` or `nrf9151`.

For example, to build and flash the blinky sample for the nRF54L15:

```shell
west build -s blinky -b nrf54l15
west flash
```

## Building and flashing with CMake directly
`west build`/`west flash` are thin wrappers around CMake and `nrfutil`, so you can use CMake and nrfutil directly if you prefer.

```shell
# 1. Configure.
cmake -B build -DSAMPLE_DIRECTORY=blinky -DTHESEUS_BUILD_TARGET=nrf54l15

# 2. Build.
cmake --build build --parallel

# 3. Additional options
cmake --build build --parallel --target flash # If you want to flash after building

cmake --build build --parallel --clean-first # If you want a clean build
```

You can also flash the resulting `build/app.elf` with `nrfutil`:

```shell
nrfutil device program --options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE --firmware build/app.elf
nrfutil device reset
```

> **Note:** Samples that build a TrustZone secure image (e.g. `modem`) also
> produce `build/lib/secure/secure.elf`. Program it *before* `app.elf`.

## Build configuration
First-party code is built with extra checks by default to catch bugs during development.
The useful build flags:

- `-p` - pristine (clean) build.
- `-DTHESEUS_UBSAN=OFF` - by default, UBSan (trap mode) catches undefined behavior (overflow, out-of-bounds shifts, null derefs, etc.) at runtime, this flag drops it for a smaller release image.
- `-DTHESEUS_FAULT_HANDLERS=OFF` - by default, HardFault/BusFault/etc. handlers print crash info over UART, this flag drops them for a smaller release image.

```shell
west build -s SAMPLE_NAME -b TARGET_BOARD -p -DTHESEUS_UBSAN=OFF -DTHESEUS_FAULT_HANDLERS=OFF
```
