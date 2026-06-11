# nrf-theseus
Base repository for the nRF Connect SDK Bare Metal option.

## Initialize workspace

To initialize the workspace using the command line, do the following:

1. Clone the relevant version tag or branch:

   ```shell
   west init -m https://github.com/sondrep/nrf-theseus.git --mr main
   ```

2. Update the structure based on the current repository revision:

   ```shell
   west update
   ```

## Building & Flashing a Sample

To build a project you can run:

```shell
west build -s SAMPLE_NAME -b TARGET_BOARD
```
This builds a sample from the `samples/` directory, where you need to replace `SAMPLE_NAME` with the directory name of the sample you want to build.
The target boards can be found in `cmake/targets/`, where you need to replace `TARGET_BOARD` with the name of one of the targets found in this directory (without `.cmake`).

After a sample is built, you can run:
```shell
west flash
```
to flash the sample you just built.
