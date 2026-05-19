# Fellowship: Skyrim Multiplayer Mod

Fellowship is a small-scale (2-5 player) multiplayer mod for Skyrim Special Edition / Anniversary Edition. It focuses on high-fidelity position synchronization using ENet (UDP) for low latency.

## Prerequisites

Before installing, ensure you have the following requirements:

- **Skyrim Special Edition or Anniversary Edition** (v1.6.1170 recommended).
- **SKSE64**: [Skyrim Script Extender](https://skse.silverlock.org/).
- **Address Library for SKSE Plugins**: [Nexus Mods](https://www.nexusmods.com/skyrimspecialedition/mods/32444).
- **Node.js**: Required for running the multiplayer server.
- **CMake**: Required for building the client-side plugin.

### Installation
1. Install [SKSE64](https://skse.silverlock.org/) and [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444).
   - For Skyrim 1.6.1170, ensure you have the "All-in-one" version 11.
2. Copy `Fellowship.dll` to `Data/SKSE/Plugins/`.
3. Copy `fmt.dll` and `spdlog.dll` to the Skyrim root folder.

### Testing
- Run the server: `node server/index.js`
- Launch Skyrim via `skse64_loader.exe`.
- The mod will automatically connect to `127.0.0.1:3000`.
- Press `F4` in-game to dump synchronization status to the console (`~`).

```bash
cd server
npm install
node index.js
```

### 2. Client Setup (Manual Installation)
To install the client plugin, place the compiled files into your Skyrim directory:

1. Locate `Fellowship.dll` in the `dist/` folder of this repository.
2. Copy it to your Skyrim directory at:
   `Skyrim Special Edition/Data/SKSE/Plugins/Fellowship.dll`

*Note: You may need to create the `SKSE/Plugins` folders if they do not already exist.*


### 3. Playing
1. Ensure the server is running (`node server/index.js`).
2. Launch Skyrim using `skse64_loader.exe`.
3. Once in-game, the plugin will attempt to connect to `127.0.0.1:3000` (localhost).

## Development and Diagnostics

### Synchronization Status
You can verify the connection and synchronization status in-game:
- **Periodic Log**: The status is printed to the console (`~`) every 10 seconds.
- **Manual Trigger**: Press **F4** to instantly print a detailed synchronization report to the console.

### Project Structure
- `/client`: C++ source code for the SKSE plugin.
- `/server`: Node.js source code for the multiplayer server.
- `/tools`: Utility scripts for development.
- `Deploy.ps1`: Automated build and deployment script.

## Technical Details
- **Networking**: ENet for reliable/unreliable UDP transport.
- **Serialization**: JSON (nlohmann-json) for all network messages.
- **Sync Rate**: Player updates are sent every 50ms.
