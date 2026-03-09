## About

```bash
scand is a lightweight TCP port scanner designed for embedded systems and constrained environments.
```

## Build

```bash
gcc -Os -s -o scand main.c          # glibc
musl-gcc -Os -s -o scand main.c     # musl
```

## Usage

```bash
# default scan (ports 1-1024)
./scand 192.168.1.1

# specific ports
./scand 192.168.1.1 22,80,443

# range
./scand 10.0.0.5 1-1024

# custom timeout
./scand 192.168.0.10 1-65535 --timeout 150
```

## Recommended Use

```bash
Embedded diagnostics.
IoT service auditing.
Firmware integration.
```
