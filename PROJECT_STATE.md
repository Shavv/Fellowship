# Fellowship: Skyrim Multiplayer Mod - Development State

## Project Overview
A small-scale (2-5 player) Skyrim multiplayer mod focusing on high-fidelity synchronization and persistent progression.

## Technology Stack
- **Client**: C++ (SKSE Plugin) using CommonLibSSE.
- **Networking**: ENet (UDP) for low-latency sync.
- **Data Format**: JSON (nlohmann-json) for extensible messaging.
- **Server**: Node.js (ENet wrapper) for message broadcasting.
- **Storage**: Firebase/Firestore (Initialized, pending integration).

## Current Architecture
### 1. Synchronization
- **Position**: Local player position (X, Y, Z, Rot) is broadcasted every 50ms with 2-decimal precision.
- **Animations**: `BSAnimationGraphEvent` hooks capture and broadcast animation tags reliably.
- **Dummy Actors**: Remote players are represented as dummy actors (cloned from the player base NPC) and interpolated between updates.
- **Cell Awareness**: Basic cell/world tracking is implemented to hide/show players across interior/exterior transitions.

### 2. Development Tools
- **Deploy.ps1**: Builds the plugin and deploys the DLL to the Skyrim directory.
- **Server**: Run manually via `node server/index.js`.

## Workflow
1. Run `.\Deploy.ps1` to build and install the plugin.
2. Start the server in a separate terminal.
3. Launch Skyrim via `skse64_loader.exe`.

## Recent Changes
- Removed all multi-instance stabilization hacks and tools.
- Simplified the testing environment to focus on core multiplayer logic.
- Integrated `nlohmann-json` for a more robust and extensible network protocol.
- Cleaned up outdated auto-teleportation (COC) remnants.

## Pending Goals
- [ ] **Combat Synchronization**: Broadcast hit events, blocks, and damage.
- [ ] **Cell/World Transition**: Refine actor persistence when moving between distant interior cells.
- [ ] **Firestore Integration**: Map player IDs to cloud profiles for persistent stats/inventory.
- [ ] **Physics/Object Sync**: Sync dropped items or moved objects.

---
*This file serves as a persistent context for the Antigravity AI assistant.*
