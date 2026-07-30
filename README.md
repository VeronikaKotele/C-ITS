# C-ITS Simulator

A C++20 simulation of a **Cooperative Intelligent Transport System (C-ITS)** following ETSI standards. The simulator models vehicles (On-Board Units) and roadside infrastructure (Roadside Units / Traffic Light Controllers) communicating over an MQTT broker using standardised V2X message types.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Features](#features)
- [Architecture](#architecture)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Usage](#usage)
- [Configuration](#configuration)
- [Directory Structure](#directory-structure)
- [Contributing](#contributing)
- [License](#license)

---

## Project Overview

C-ITS (Cooperative Intelligent Transport Systems) enable vehicles and road infrastructure to exchange real-time information, improving traffic flow and safety. This project simulates that communication locally:

- **Vehicles** broadcast their position and request signal priority at intersections.
- **Roadside Units (RSUs)** act as traffic-light controllers that receive vehicle data, decide whether to grant priority, and broadcast signal phase/timing information.
- A **WebSocket bridge** streams simulation state to external clients (e.g. a visualisation dashboard) on port 8080.

All V2X messages are encoded with [Protocol Buffers](https://protobuf.dev/) and transported over MQTT, mirroring real-world C-ITS deployments.

---

## Features

- Simulation of multiple simultaneous vehicle types: **Car**, **Bus**, **Emergency**
- ETSI-compliant V2X message types:
  - **CAM** – Cooperative Awareness Message (vehicle position & kinematics)
  - **SREM** – Signal Request Extended Message (priority request from vehicle)
  - **SSEM** – Signal Status Extended Message (priority response from RSU)
  - **SPATEM** – Signal Phase and Timing Extended Message (traffic-light state)
- Priority decision logic in the RSU (grants or denies based on vehicle role and proximity)
- Duplicate-message deduplication with automatic cache expiry
- Real-time WebSocket broadcast of simulation state (JSON)
- Multi-threaded design: each RSU and vehicle runs in its own thread
- Cross-platform CMake build with presets for Windows, Linux, and macOS

---

## Architecture

```
┌─────────────┐   CAM / SREM   ┌────────────────────┐
│  Vehicle    │ ─────────────► │                    │
│  (OBU)      │ ◄───────────── │  Eclipse Mosquitto │
│  Bus /      │   SSEM /       │  MQTT Broker       │
│  Car /      │   SPATEM       │  (tcp://localhost:  │
│  Emergency  │                │       1883)        │
└─────────────┘                └────────┬───────────┘
                                        │ CAM / SREM
                                        ▼
                               ┌─────────────────────┐
                               │  Roadside Unit (RSU)│
                               │  Traffic Controller │
                               └─────────────────────┘
                                        │
                               ┌────────▼────────────┐
                               │  WebSocket Bridge   │
                               │  ws://localhost:8080│
                               └─────────────────────┘
```

Each vehicle and RSU connects to the broker as an MQTT client. The RSU subscribes to CAM and SREM topics, processes priority requests, and publishes SSEM and SPATEM responses. The WebSocket bridge broadcasts JSON snapshots of the simulation state to any connected client.

---

## Prerequisites

| Dependency | Minimum version | Notes |
|---|---|---|
| C++ compiler | C++20 | GCC 11 / Clang 14 / MSVC 2022 |
| CMake | 3.20 | |
| Ninja | any | Used by CMake presets |
| Protocol Buffers (`protobuf`) | 3.x | `libprotobuf` + `protoc` |
| Eclipse Paho MQTT C | 1.3 | `paho-mqtt3as` |
| Eclipse Paho MQTT C++ | 1.2 | `paho-mqttpp3` |
| nlohmann/json | 3.x | |
| Asio (standalone) | 1.18 | **Without** Boost |
| Eclipse Mosquitto broker | 2.x | Must be running before the simulator starts |

> **WebSocket++** – a patched copy compatible with C++20 is already included under `C-ITS/third_party/websocketpp`. Do **not** replace it with an unpatched upstream version (see `C-ITS/CMakeLists.txt` comments for details).

### Installing dependencies on Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build \
    protobuf-compiler libprotobuf-dev \
    libpaho-mqtt-dev libpaho-mqttpp-dev \
    nlohmann-json3-dev \
    libasio-dev \
    mosquitto mosquitto-clients
```

### Installing dependencies on Windows

Use [vcpkg](https://github.com/microsoft/vcpkg):

```powershell
vcpkg install protobuf paho-mqtt paho-mqttpp3 nlohmann-json asio
```

Start Mosquitto as a Windows service or run `mosquitto.exe` manually.

---

## Installation

```bash
# 1. Clone the repository
git clone https://github.com/VeronikaKotele/C-ITS.git
cd C-ITS

# 2. Configure (choose a preset that matches your platform)
cmake --preset linux-debug      # Linux
# cmake --preset x64-debug      # Windows x64
# cmake --preset macos-debug    # macOS

# 3. Build
cmake --build out/build/linux-debug
```

The compiled executable is placed at `out/build/<preset>/C_ITS` (Linux/macOS) or `out/build/<preset>/C_ITS.exe` (Windows).

---

## Usage

1. **Start the MQTT broker** (Mosquitto must be running on `tcp://localhost:1883`):

   ```bash
   mosquitto -v
   ```

2. **Run the simulator**:

   ```bash
   ./out/build/linux-debug/C_ITS
   ```

   Expected output:

   ```
   Starting WebSocket bridge
   Starting C-ITS simulation...
   Press Enter to stop simulation...
   ```

3. **Connect a WebSocket client** to `ws://localhost:8080` to receive real-time JSON state updates from the simulation.

4. **Stop the simulation** by pressing **Enter** in the terminal. All threads will be joined gracefully.

---

## Configuration

| Setting | Location | Default | Description |
|---|---|---|---|
| MQTT broker address | `C-ITS/src/constants.h` | `tcp://localhost:1883` | Broker URI used by all clients |
| WebSocket port | `C-ITS/src/C-ITS.cpp` (`main`) | `8080` | Port the WebSocket bridge listens on |
| RSU spawn location | `C-ITS/src/C-ITS.cpp` (`main`) | `48.776, 9.183` | Geographic coordinates of the RSU |
| Vehicle start positions | `C-ITS/src/C-ITS.cpp` (`main`) | See source | Latitude/longitude/speed per vehicle |
| Vehicle path waypoints | `C-ITS/src/C-ITS.cpp` (`main`) | See source | Ordered list of `SpawnLocation` structs |

---

## Directory Structure

```
C-ITS/
├── CMakeLists.txt              # Top-level CMake (delegates to C-ITS/)
├── CMakePresets.json           # Build presets (Windows/Linux/macOS)
└── C-ITS/
    ├── CMakeLists.txt          # Build definition for the executable
    ├── notes-n-abbreviations.txt
    ├── protobuf/               # .proto message schemas
    │   ├── etsi_common.proto
    │   ├── cam.proto
    │   ├── srem.proto
    │   ├── ssem.proto
    │   └── spatem.proto
    ├── src/                    # C++ source files
    │   ├── C-ITS.cpp / .h      # Entry point & simulation orchestration
    │   ├── Vehicle.cpp / .h    # On-Board Unit (vehicle) logic
    │   ├── RoadsideUnit.cpp / .h   # RSU / traffic-light controller
    │   ├── MqttClient.cpp / .h     # MQTT base client wrapper
    │   ├── WebSocketBridge.cpp / .h # WebSocket broadcast server
    │   ├── InterfacesTranslator.cpp / .h # ETSI enum helpers
    │   ├── TrafficController.cpp   # Traffic-light phase management
    │   ├── ProcessMessageQueue.h   # Thread-safe processing queue
    │   ├── SendMessageQueue.h      # Thread-safe send queue
    │   ├── constants.h             # Shared constants (broker address, etc.)
    │   ├── utils.cpp / .h          # Utility functions
    │   └── Interfaces/             # Shared data structures
    │       ├── VehicleTypes.h      # VehicleType enum
    │       ├── VehicleState.h      # Position, speed, heading
    │       ├── RsuState.h          # RSU traffic-light state
    │       └── SpawnLocation.h     # Geographic coordinate pair
    └── third_party/
        └── websocketpp/            # C++20-patched WebSocket++ headers
```

---

## Contributing

Contributions, bug reports, and feature requests are welcome. Please open an issue or submit a pull request on [GitHub](https://github.com/VeronikaKotele/C-ITS).

1. Fork the repository and create a feature branch.
2. Make your changes and ensure the project builds without warnings.
3. Open a pull request with a clear description of the change.

---

## License

This project does not currently include an explicit license file. Please contact the repository owner for usage terms.
