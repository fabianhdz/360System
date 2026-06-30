# 360System

360System is an RP2040-based directional proximity-awareness prototype. Five ultrasonic sensors monitor separate areas around the user, and each sensor is paired with a coin vibration motor. When an object moves closer, the corresponding motor provides directional haptic feedback. A shared PWM audio output can announce the measured distance for the primary sensor.

The firmware is written in C using the Raspberry Pi Pico SDK. It uses fixed memory, non-blocking sensor scheduling, interrupt-based echo capture, median filtering, and a dedicated second core for audio playback.

## Features

- Five independently configured ultrasonic sensors
- Five direction-specific vibration motor outputs
- Sequential sensor triggering to reduce ultrasonic crosstalk
- Non-blocking trigger, echo, timeout, and cooldown state machine
- Three-sample median distance filter
- Validation of measurements from 2 cm through 400 cm
- Movement-based motor activation with hysteresis
- Spoken distance clips from one through ten feet
- PWM audio playback on the RP2040's second core
- Compile-time configurable UART and USB diagnostic logging

## Runtime Behavior

The ultrasonic driver measures one sensor at a time. Each successful echo produces a distance event that is validated and passed through a three-sample median filter.

A motor turns on when the filtered distance decreases by at least 10 cm and the detected object is within approximately 3.1 m. It turns off when the closing distance falls to 3 cm or less, when the object moves outside that range, or when the sensor times out. This behavior provides feedback for relative movement without continuously vibrating for a stationary nearby object.

Sensor 1(Front-Most) supplies the shared audio feedback. Valid distances from one through ten whole feet select the corresponding stored speech clip. The other sensors currently provide directional motor feedback only.

## Pin Assignment

The table reflects the current firmware configuration in `360System.c`.

| Channel | Trigger | Echo | Motor |
|---|---:|---:|---:|
| Sensor 1 | GP0 | GP1 | GP2 |
| Sensor 2 | GP3 | GP4 | GP5 |
| Sensor 3 | GP6 | GP7 | GP8 |
| Sensor 4 | GP18 | GP17 | GP16 |
| Sensor 5 | GP20 | GP21 | GP19 |

Additional pins:

| Function | Pin |
|---|---:|
| PWM audio output | GP27 |
| Status LED | GP25 |

## Firmware Architecture

| Component | Responsibility |
|---|---|
| `360System.c` | Application configuration, filtering, motor policy, audio selection, and system orchestration |
| `ultrasonic.c/.h` | GPIO setup, echo interrupts, sequential acquisition, timeouts, and distance events |
| `audio.c/.h` | Interrupt-driven PWM audio playback |
| `sounds.c/.h` | Immutable speech sample data stored in flash |
| `log.h` | Compile-time diagnostic logging abstraction |
| `CMakeLists.txt` | Pico SDK target and build configuration |

Core 0 runs sensor acquisition and application policy. Core 1 waits for audio commands and plays the selected speech clip. PWM wrap interrupts are enabled only during playback.


## Prerequisites

- Raspberry Pi Pico SDK 1.4 or later
- CMake 3.13 or later
- Ninja
- Python 3
- Arm GNU Toolchain (`arm-none-eabi-gcc`)

Set `PICO_SDK_PATH` to the SDK installation:

```bash
export PICO_SDK_PATH="$HOME/pico/pico-sdk"
```

## Building

Configure and build a development image with diagnostic logging:

```bash
cmake -S . -B build-pico -G Ninja \
  -DPICO_BOARD=pico \
  -DENABLE_DEBUG_LOGS=ON

cmake --build build-pico
```

The flashable image is generated at:

```text
build-pico/360System.uf2
```

For a production-style build without UART/USB diagnostic logging:

```bash
cmake -S . -B build-pico -G Ninja \
  -DPICO_BOARD=pico \
  -DENABLE_DEBUG_LOGS=OFF

cmake --build build-pico
```

Generated build directories are excluded through `.gitignore`.

## Flashing

1. Disconnect the Pico from USB.
2. Hold the `BOOTSEL` button while reconnecting it.
3. Release `BOOTSEL` after the `RPI-RP2` mass-storage device appears.
4. Copy `build-pico/360System.uf2` to the device.


