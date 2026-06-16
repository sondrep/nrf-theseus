# nrf-theseus
Base repository for the nRF Theseus project — a bare-metal FreeRTOS port for
the nRF54L15 (Arm Cortex-M33), built and managed with west.

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
