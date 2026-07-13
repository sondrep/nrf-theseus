# nrf-theseus
Base repository for the nRF Theseus project — a bare-metal FreeRTOS port for
the nRF54L15 (Arm Cortex-M33), built and managed with west.

--------------------------------------------------

<br>

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

## Build and flashing a sample
```shell
cd ~/my_ws/nrf-theseus
west build -s SAMPLE_NAME -b TARGET_BOARD
west flash
```

- `SAMPLE_NAME` — a directory under `samples/`.
- `TARGET_BOARD` — a target from `cmake/targets/`, without the `.cmake` suffix.

> **Note:** The NimBLE build compiles Apache Mynewt's `newt` tool from source, which requires Go. Install it before building:
>
> ```shell
> sudo apt install golang-go
> ```

> **Note:** Building requires the `arm-none-eabi` GCC toolchain (targets the Cortex-M33 on nRF54L/nRF91). Install it before building:
>
> ```shell
> sudo apt install gcc-arm-none-eabi
> ```

--------------------------------------------------

<br>

## Build configuration
First-party code is built with extra checks by default to catch bugs during development.
The useful build flags:

- `-p` - pristine (clean) build.
- `-DTHESEUS_UBSAN=OFF` - by default, UBSan (trap mode) catches undefined behavior (overflow, out-of-bounds shifts, null derefs, etc.) at runtime, this flag drops it for a smaller release image.
- `-DTHESEUS_FAULT_HANDLERS=OFF` - by default, HardFault/BusFault/etc. handlers print crash info over UART, this flag drops them for a smaller release image.

```shell
west build -s SAMPLE_NAME -b TARGET_BOARD -p -DTHESEUS_UBSAN=OFF -DTHESEUS_FAULT_HANDLERS=OFF
```
