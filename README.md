# MAVPad Bridge

`MAVPad Bridge` is an ESP32-based control bridge that turns a Bluetooth gamepad into MAVLink RC override traffic, SBUS output, and a Wi-Fi UDP link for MAVLink-enabled vehicles and companion devices.

The project is built around an Arduino sketch and a bundled MAVLink C header set. In its current form, it behaves like a lightweight ground-station/controller bridge:

- Reads Bluetooth gamepad input using `Bluepad32`
- Maps sticks and triggers into RC-style channel values
- Sends `RC_CHANNELS_OVERRIDE` over MAVLink
- Outputs the same channel values over SBUS
- Sends MAVLink heartbeat packets over UDP
- Receives and decodes incoming UDP MAVLink packets for debugging

## What This Project Is For

This project is intended for setups where an ESP32 acts as a bridge between:

- A Bluetooth gamepad
- A MAVLink-capable flight stack, air unit, or companion device
- An SBUS-based downstream receiver/input path
- A peer ESP32 or networked endpoint over Wi-Fi UDP

In short:

`Bluetooth gamepad -> ESP32 -> MAVLink RC override + SBUS + UDP transport`

## Current Behavior

The main firmware lives in `Base_code_v4.ino`.

At runtime, the firmware:

1. Starts Bluetooth controller support with `Bluepad32`
2. Connects the ESP32 to Wi-Fi with a static IP
3. Opens a UDP listener on port `5005`
4. Sends periodic MAVLink heartbeat messages to a configured peer IP
5. Reads controller inputs and maps them to channel values
6. Packs those values into a MAVLink `RC_CHANNELS_OVERRIDE` message
7. Transmits the packed MAVLink message over `Serial2` and UDP
8. Copies the same channel values into SBUS output frames

## Repository Layout

- `Base_code_v4.ino`: Main firmware sketch
- `GamePad.cpp` / `GamePad.h`: Bluetooth gamepad connection and channel mapping
- `get_mavlink_message.cpp` / `get_mavlink_message.h`: MAVLink message ID to name helper for debug logs
- `message_definitions/`: MAVLink XML dialect definitions
- `common/`, `standard/`, `ardupilotmega/`, `all/`, etc.: Generated MAVLink headers bundled with the project
- `mavlink_*.h`, `protocol.h`, `checksum.h`: MAVLink support headers

Most of the repository is generated MAVLink protocol code. The custom project logic is concentrated in the Arduino sketch and the gamepad helper files.

## Hardware / Software Stack

- ESP32
- Arduino framework
- `Bluepad32` for Bluetooth gamepad input
- `sbus` library for SBUS RX/TX
- MAVLink C headers bundled in this repository
- Wi-Fi UDP transport

## Network Configuration

The current sketch is configured with hardcoded network values:

- Local static IP: `192.168.1.101`
- Peer IP: `192.168.1.102`
- Gateway: `192.168.1.1`
- UDP port: `5005`

Wi-Fi credentials are also hardcoded in `Base_code_v4.ino`. If this repository is going public on GitHub, those credentials should be changed or removed before publishing.

## Input Mapping

The gamepad helper maps controller inputs into `channel[16]` values:

- `axisRX`, `axisRY`, `axisY`, `axisX` -> first 4 RC channels
- `brake`, `throttle` -> channels 5 and 6
- Remaining channels default to a neutral value

The resulting values are then used for:

- MAVLink `RC_CHANNELS_OVERRIDE`
- SBUS output frames

## Build Notes

This repository does not currently include:

- `platformio.ini`
- Arduino library manifest files
- wiring documentation
- release packaging

So to build it, you will need to:

1. Open the project in the Arduino IDE or an ESP32-compatible Arduino workflow
2. Install the required libraries manually:
   - `Bluepad32`
   - an `sbus` library compatible with `bfs::SbusRx` / `bfs::SbusTx`
3. Select the correct ESP32 board and serial settings
4. Review UART pin usage before flashing

## Status

This looks like a working prototype rather than a fully finished product.

Notable implementation details:

- UDP receive and MAVLink decode are implemented for debugging
- Heartbeat transmission is implemented
- Controller-to-MAVLink and controller-to-SBUS output are implemented
- UART-to-UDP MAVLink forwarding appears incomplete and placeholder in `receiveMAVLinkData()`

## Suggested Improvements

- Move Wi-Fi credentials and network settings into a separate config header
- Add a proper build environment file such as `platformio.ini`
- Document board selection and wiring
- Clarify the intended `Serial2`/SBUS hardware topology
- Complete the UART-to-UDP MAVLink forwarding path
- Add a license file
- Add diagrams for data flow and pin connections

## Suggested GitHub Description

`ESP32 Bluetooth gamepad to MAVLink RC override, SBUS output, and Wi-Fi UDP bridge for drone control and telemetry experiments.`

## Suggested Topics

`esp32`, `arduino`, `mavlink`, `sbus`, `bluepad32`, `drone`, `uav`, `ground-station`, `gamepad`, `udp`

