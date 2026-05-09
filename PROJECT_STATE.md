# Fellowship: Skyrim Multiplayer Mod - Development State

## Project Overview
A small-scale (2-5 player) Skyrim multiplayer mod focusing on stable multi-instance execution and real-time synchronization.

## Technology Stack
- **Client**: C++ (SKSE Plugin) using CommonLibSSE.
- **Networking**: ENet (UDP) for low-latency position and animation syncing.
- **Server**: Node.js (ENet wrapper) for message broadcasting.
- **Storage**: Firebase/Firestore (Initialized for persistent data, but not yet integrated into the sync loop).

## Current Architecture
### 1. Multi-Instance Stabilization
- **Handle Spoofing**: Patches `GetModuleHandleA` to handle cases where the EXE is renamed (e.g., `SkyrimSE2.exe`).
- **Mutex Management**: Bypasses the `Global\SkyrimSE` mutex and internal engine checks to allow multiple instances on a single Windows machine.
- **Secondary User**: Uses a dedicated Windows account (`Fellowship`) to launch the second client instance via `Start-Process -Credential`.

### 2. Synchronization
- **Position**: Local player position (X, Y, Z, Rot) is broadcasted every 50ms.
- **Animations**: `BSAnimationGraphEvent` hooks capture and broadcast animation tags.
- **Dummy Actors**: Remote players are represented as dummy actors (cloned from the player base NPC) and interpolated between updates.

### 3. Testing Automation
- **Start-TestEnv.ps1**: Builds the plugin, starts the server, and launches two clients with a timed lifecycle and automated shutdown.
- **Stop-TestEnv.ps1**: Force-kills all relevant processes.

## Workflow
To run a test session:
```powershell
.\Start-TestEnv.ps1 -TestDuration 60
```

## Recent Changes
- Removed the automated `coc whiterun` trigger to allow manual save loading.
- Implemented a countdown-based automation script for starting/stopping the environment.
- Initialized Firebase/Firestore in the project root.

## Pending Goals
- [ ] **Firestore Integration**: Map player IDs to cloud profiles for persistent stats/inventory.
- [ ] **Cell/World Transition**: Improve actor management when players move between interior and exterior cells.
- [ ] **Combat Synchronization**: Broadcast hit events and damage.
- [ ] **Physics/Object Sync**: Sync dropped items or moved objects.

---
*This file serves as a persistent context for the Antigravity AI assistant.*
