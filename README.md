# ESP32 OBD-II Logger

An OBD-II diagnostic data logger built for the ESP32 platform using PlatformIO.

## Overview

This project aims to provide a reliable way to capture and log vehicle diagnostic data (OBD-II) from a car's ECU using an ESP32 microcontroller communicating with a dongle via Bluetooth.

## Features

- OBD-II protocol support (via CAN bus).
- **Bluetooth BLE Support**: Initial support for Bluetooth Low Energy (BLE) OBD-II dongles, specifically tested with the [LELink Bluetooth Low Energy BLE OBD-II OBD2 Car Diagnostic Tool](https://a.co/d/04KAmFuA).
- [Future] Data logging to local storage.
- Status, configuration & control via Wi-Fi (Web UI).

## Prerequisites

- **Hardware**:
  - ESP32 Development Board.
  - BLE OBD-II Dongle.
- **Software**:
  - [PlatformIO](https://platformio.org/) installed (VS Code extension recommended).

## Setup

1. Clone this repository.
2. Open the project folder in VS Code with the PlatformIO extension.
3. Build and upload the project to your ESP32 board using the PlatformIO "Upload" task.

## Project Structure

- `src/`: Main source code.
- `include/`: Header files.
- `lib/`: External dependencies.
- `platformio.ini`: PlatformIO configuration file.

## Contributing

Contributions are welcome! Please feel free to open an issue or submit a pull request.

## License

[Add your license here, e.g., MIT, GPLv3]
