# Fellowship: Skyrim Multiplayer Mod - Development State

## Project Overview
A small-scale (2-5 player) Skyrim multiplayer mod focusing on high-fidelity synchronization and persistent progression.

## Development Rules
1. **Never adjust Address Library files**: These are core dependencies and must remain in their original state to ensure compatibility.
2. **Never adjust SKSE files**: The Skyrim Script Extender and its core components are fixed; all mod logic must reside within the Fellowship plugin or server.

## Technology Stack
- **Client**: C++ (SKSE Plugin) using CommonLibSSE.
- **Networking**: ENet (UDP) for low-latency sync.
- **Data Format**: JSON (nlohmann-json) for extensible messaging.
- **Server**: Node.js (ENet wrapper) for message broadcasting.
- **Storage**: Firebase/Firestore (Initialized, pending integration).

## Current Architecture
### 1. Synchronization
- **Position**: Local player position (X, Y, Z, Rot) is broadcasted every 50ms with 2-decimal precision.
- **Animations**: [REMOVED] All animation synchronization logic (discrete events and state-based) has been removed to focus on movement and combat stability.
- **Dummy Actors**: [ACTIVE] Remote players are represented as dummy actors (cloned from player base NPC) with physics/AI frozen but position interpolated. Currently a primary focus for testing.
- **Cell Awareness**: Basic cell/world tracking is implemented to hide/show players across interior/exterior transitions.

### 2. Development Tools
- **Deploy.ps1**: Builds the plugin and deploys the DLL to the Skyrim directory.
- **Server**: Run manually via `node server/index.js`.

## Workflow
1. Run `.\Deploy.ps1` to build and install the plugin.
2. Start the server in a separate terminal.
3. Launch Skyrim via `skse64_loader.exe`.

## Recent Changes
- Fixed server diagnostic logic to correctly track cell/world IDs.
- [x] **Startup Stability**: [RESOLVED] Fixed infinite tight loop in heartbeat by decoupling from SKSE task queue recursion.
- [x] **Automatic Connection**: [RESOLVED] Plugin now connects to localhost:3000 automatically on startup.
- [x] **Diagnostic Logging**: [RESOLVED] Server now logs remote dummy spawning events from clients.
- [x] **Console Notifications**: [RESOLVED] Added in-game console messages for plugin load, connection, and disconnection.
- [x] **Havok Physics Corruption**: [RESOLVED] Fixed infinite water planes stretching and black rectangle graphical glitches caused by `SetPosition(pos, false)`. Passing `true` now correctly updates the physics bounding box.
- [x] **Bridge Swimming Bug**: [RESOLVED] Set waterHeight to -30000.0f and gravity to 0.0f on all remote actor controllers.
- [x] **Main Menu & Connection Notifications**: [RESOLVED] Verified in-game console notifications and connections on a new game.
- [x] **Swimming Above Land Glitch**: [RESOLVED] Fixed physics repair routine in `Hooks.cpp` which was mistakenly flagging the normal dry-land sentinel water height (-30000.0f) as a corrupted state and continuously overriding it, breaking Havok's sensors.

## Current Focus: Dummy Spawning & Reliability
- **Goal**: Achieve 100% reliable actor proxy management and smooth movement.
- **Tasks**:
  - [x] Remove animation/combat noise from synchronization.
  - [ ] Test movement smoothness and actor spawning consistency.
  - [ ] **Self-Sync Jerking**: [PENDING LIVE TEST] Implemented Atomic ID Guard, Physical Proximity Filter, and Strict Normalization.
  - [x] **Distance-Based Cleanup**: [RESOLVED] Actors > 15k units are disabled; > 60k units are removed from tracking.
  - [x] **Micro-Jitter Prevention**: [RESOLVED] Added threshold check to prevent position updates for negligible movements (< 0.1 units).
  - [x] **Bridge Swimming Bug**: [RESOLVED] Set waterHeight to -30000.0f and gravity to 0.0f on all remote actor controllers.
  - [x] **Havok Physics Corruption**: [RESOLVED] Fixed infinite water planes stretching and black rectangle graphical glitches by re-enabling Havok position sync (`SetPosition(pos, true)`) while retaining character controller collision suppression flags.
  - [ ] Refine actor persistence when moving between distant interior cells.

## Pending Goals (Long-term)
- [ ] **Animation Synchronization**: RE-INTRODUCE ONLY after achieving perfect movement stability.
- [ ] **Combat Synchronization**: RE-INTRODUCE ONLY after achieving perfect movement stability.
- [ ] **Physics/Object Sync**: Sync dropped items or moved objects.

---
*Last updated: 2026-05-19*
*This file serves as a persistent context for the Antigravity AI assistant.*
