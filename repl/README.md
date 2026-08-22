# REPL

The μλ REPL (Read, Evaluate, Print, Loop). Allows for line by line code execution or batch code upload for quick testing.

## Setup

Make sure that zephyr toolchain is installed!!!

## Building

test build in native sim:
```
west build -b native_sim -d build .
```
build for some zephyr board:
```
west build -b [BOARD] -d [BUILD_DIRECTORY] .
```

## Running on native_sim

```
./build/zephyr/zephyr.exe -uart_stdinout
```

## Flashing

```
ls /dev/ttyACM*
west flash -d [BUILD_DIRECTORY] /dev/ttyACMx
```

Some runners need to be told which device to use, e.g. `--esp-device /dev/ttyACMx`.

## Interacting with the REPL

```
screen /dev/ttyACMx 115200
```

| key | effect |
| --- | --- |
| ctrl-c | cancel the line, or interrupt a running evaluation |
| ctrl-e | enter batch mode (on an empty line) |
| ctrl-d | run the batch |
| tab | indent by four spaces |

Line mode evaluates one form at a time. A line ending in `:` opens a block and
keeps reading until a blank line. Batch mode collects everything until ctrl-d
and evaluates it as one unit.

## Sending a file

`upload.sh` wraps a file in the batch mode control characters and prints what
comes back.

Print output to terminal:
```
./upload.sh program.txt /dev/ttyACMx
```
Pipe output to a file:
```
./upload.sh program.txt /dev/ttyACMx > out.txt
```
