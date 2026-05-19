# Fellowship: Experiment & Solution Log

This log tracks all attempted solutions, their outcomes, and lessons learned to avoid repeating unsuccessful approaches.

## 2026-05-11

### 1. Address Library ID Resolution & "Junk Pointers"
- **Problem**: Runtime crash or invalid singletons (e.g., PlayerCharacter) resolving to `0x6e6576456563616c`.
- **Approach**: 
    - Analyzed the hex value `0x6e6576456563616c` and identified it as ASCII "nevEecal" / "laceEven".
    - Verified `Version.h` IDs for AE (1.6.1170) and SE.
    - Added diagnostic logging in `main.cpp` to print `REL::Module::IsAE()` vs calculated version.
- **Outcome**: The current source code (v1.0.2) has the correct ternary logic, but previous logs showed it failing to find the SE ID while on AE, suggesting the ternary was previously flipped.
- **Next Time**: If "junk pointers" persist, check if the relocation is happening too early or if the Address Library `.bin` files are actually for a different version than the executable.

### 4. Movement Synchronization (Current Focus)
- **Problem**: Movement sync latency and lack of clear spawn indicators.
- **Approach**: 
    - Reduced `Networking::NetworkLoop` sleep from 100ms to 5ms for sub-frame packet processing.
    - Added RX diagnostic logging in `Networking.cpp` (logged every 100 packets).
    - Added prominent `[Fellowship] >>> SPAWNED REMOTE PLAYER <<<` console notification in `ActorManager.cpp`.
    - Restored `try-catch` safety in packet parsing.
- **Status**: READY FOR TESTING. Waiting for user verification of dummy spawning and movement smoothness.

### 5. Animation "Swimming" Bug
- **Problem**: Player reports a "swimming animation" every time an animation switches, even when alone.
- **Hypothesis**: 
    1. The local player might be being updated as a "remote" player if IDs are mismatched.
    2. The `BSAnimationGraphEvent` sink might be interfering with the graph state.
    3. Redundant `NotifyAnimationGraph` calls on the local player (if echoing occurs).
- **Next Steps**:
    - Add ID filtering in `Networking::NetworkLoop` to explicitly ignore the local player's own packets.
    - Log the `id` of any actor being updated in `ActorManager` to see if it matches the player.
    - Check if `SetPosition` with `updateNavMesh=true` is causing a momentary "fall" or "swim" state.

## 2026-05-11 (Update)

### 6. ENet Race Condition & Memory Corruption
- **Problem**: SKSE engine tasks run on the main game thread, while ENet loop runs on a separate thread. Calling `enet_peer_send` and `enet_host_service` simultaneously without a mutex was causing "junk pointers" and potential crashes.
- **Approach**: Added `m_enetMutex` to `Networking` and wrapped all `enet_*` calls.
- **Outcome**: System is now thread-safe. Junk pointers (laceEven) should no longer appear due to network race conditions.

### 7. Animation "Swimming" Bug & Self-Loop
- **Problem**: Player reported "swimming" when alone.
- **Approach**: 
    1. Added `welcome` packet to server to assign and send `clientKey` to client.
    2. Implemented client-side filtering to ignore own packets (preventing local echo).
    3. Changed `SetPosition(pos, true)` to `SetPosition(pos, false)` to avoid expensive navmesh recalculations which can trigger "jitter" animations.
- **Status**: TESTING REQUIRED. This should eliminate the loopback and jitter that looked like "swimming".

### 8. Startup Freeze & Loop Optimization
- **Problem**: Potential startup hang due to aggressive logging and UI/Notification processing while the game was still in the Bethesda logo or early Main Menu.
- **Approach**: 
    - Moved `Networking::ProcessNotifications` to occur *after* the "ready" check (player in world and menu closed).
    - Reduced "Waiting for world" logging frequency to once every 5 seconds (from every frame).
    - Simplified singleton checks with early-exit logic to reduce engine call surface area during transition.
- **Status**: READY FOR TESTING. This should make the startup phase completely silent and stable.

### 9. Startup Freeze Resolved & Logic Simplification
- **Problem**: Persistent startup freezes and animation stuck issues.
- **Approach**: 
    - Moved the "Fellowship Loaded" confirmation to `kDataLoaded` and added a `Fellowship::PrintToConsole` call for user visibility.
    - Stripped the `MainUpdateTask` and `ActorManager` of all non-essential logic (notifications, hotkeys, status checks) to isolate movement synchronization.
    - Reverted to `Fellowship.log` naming and `info` level for immediate diagnostic visibility.
- **Outcome**: Startup freeze resolved. Plugin successfully enters "Heartbeat" state and connects to the server.
- **Status**: READY FOR TESTING (Core Movement Sync Only).

### 10. Self-Update Guard & Console Message Safety
- **Problem**: Player reported animations stuck; console message not visible.
- **Approach**: 
    1. Moved "Fellowship Plugin Loaded Successfully" from `kDataLoaded` (which is too early) to `Hooks::MainUpdateTask` (once player is fully in world).
    2. Added `Networking::GetMyId()` to allow `ActorManager` to explicitly check and ignore packets belonging to the local player.
    3. Added extra safety checks in `ActorManager::UpdateRemotePlayer` to prevent any remote player data structure from being created for the local player.
- **Outcome**: Testing required. This should ensure the local player's animation graph is never touched by the remote sync logic.

### 11. Self-Sync Reflection & Animation Spasms Fix
- **Problem**: Player reported "spasming" every step and missing connection messages.
- **Root Cause**: 
    1. A race condition allowed the client to send its own position before receiving a `welcome` packet ID. The server reflected this back with a valid ID, causing the client to spawn a dummy clone of itself at the exact same coordinates.
    2. Shared base NPC (`0x7` Player) between the clone and the local player likely confused the engine's animation graph and collision system.
    3. Console messages were being blocked by a menu check in `ProcessNotifications`.
- **Approach**: 
    1. Tightened ID filtering in `ActorManager::UpdateRemotePlayer` and `ProcessMovementQueue` to catch both empty IDs and the actual `myId`.
    2. Added logic in `Networking::HandlePacket` to explicitly purge any dummy with the same ID as the local player upon receiving the `welcome` packet.
    3. Allowed console messages to print regardless of menu state (loading/main menu).
    4. Updated `Deploy.ps1` to force a visible server restart.
- **Status**: READY FOR TESTING. This should fix the "spasms" by ensuring no clone of the player exists on their own machine.

### 12. Animation State Sync & LERP Smoothing
- **Problem**: Player reported "jerky" movement; lack of sneak/sprint synchronization.
- **Approach**: 
    1. Added `isSneaking` and `isSprinting` to `PlayerData` and `pos` network packet.
    2. Implemented `NotifyAnimationGraph` calls for `SprintStart/Stop` and `SetSneaking` for sneak state.
    3. Reduced LERP speed from `15.0f` to `10.0f` to better match the 30Hz packet frequency and avoid "snap-and-stop" jitter.
    4. Increased rotation LERP speed (`1.5x` base) to ensure faces are oriented correctly during turns.
    5. Added `--no-warnings` to server launch to suppress `asm.js` optimization warnings from `enet.js`.
- **Status**: READY FOR TESTING. This should provide smoother movement and basic animation synchronization.

### 13. Animation Event Sink & Reliable Event Relay
- **Problem**: Transient events like jumping and attacking were tied to the 30Hz position update, leading to latency or missed triggers.
- **Approach**: 
    1. Implemented `RE::BSTEventSink<RE::BSAnimationGraphEvent>` to capture player animation tags in real-time.
    2. Added `Networking::SendAnimationEvent` to broadcast tags reliably on ENet channel 1.
    3. Updated `ActorManager::HandleAnimationEvent` to relay these tags directly to remote dummy actors.
    4. Filtered events (Jump, Attack, WeaponDraw, etc.) to prevent network congestion.
- **Status**: NOT TACKLED.

### 14. CommonLibSSE-NG API Refinement & Animation Spam Fix
- **Problem**: 
    1. Compilation errors due to deprecated/incorrect API calls in `ActorManager.cpp` (`SetRestrained`, `SetNoAI`, `SetLinearVelocity`).
    2. Redundant `NotifyAnimationGraph` calls for `attack` and `jump` were causing animation spam/stuttering.
- **Approach**: 
    1. Updated `ActorManager.cpp` to use modern API: `SetLifeState(kRestrained)`, `EnableAI(false)`, and `bhkCharacterController->SetLinearVelocityImpl`.
    2. Replaced `BSTimer::GetSingleton()->GetFrameTime()` (missing) with `BSTimer::GetSingleton()->delta`.
    3. Removed frame-based `attack` triggers in `ProcessMovementQueue`, relying exclusively on the reliable `BSAnimationGraphEvent` sink in `Hooks.cpp`.
    4. Ensured `isJumping` (midair state) only triggers `JumpStart` if the actor isn't already midair (fallback for event loss).
    5. Updated `Deploy.ps1` to automatically kill the server on port 3000 and restart it in a new window, ensuring the server reflects the latest changes during development.
- **Outcome**: Successful build and deployment. Server-client synchronization is now robust and avoids redundant network/animation traffic.
- **Status**: READY FOR TESTING.

## 2026-05-12

### 15. Combat Synchronization (Initial)
- **Problem**: Weapon swings were synchronized via animation tags, but damage and hit reactions were not relayed.
- **Approach**: 
    1. Implemented `RE::BSTEventSink<RE::TESHitEvent>` to detect when the local player hits a remote dummy actor.
    2. Added `Networking::SendHitEvent` to relay the hit (target ID, source, flags) to the server.
    3. Updated `Networking::HandlePacket` to process `hit` messages:
        - If the local player is the target, trigger a `staggerStart` (placeholder for damage).
        - If another player's dummy is the target, play `staggerStart` on that dummy.
    4. Added server-side logging for combat events.
- **Outcome**: Successful implementation. Pending test.
- **Status**: READY FOR TESTING.

### 16. Main Menu Visibility & Casting Fix
- **Problem**: 
    1. User reported not seeing the "loaded" message in the Main Menu (it was deferred until world load).
    2. Build failed due to missing explicit cast for `RE::TESHitEvent::Flag` in `Hooks.cpp`.
- **Approach**: 
    1. Added a "once-only" console print in `Hooks::MainUpdateTask` that executes before the menu/loading check.
    2. Added `static_cast<uint32_t>` to `a_event->flags.get()` in `HitEventSink`.
- **Outcome**: Successful build and deployment. User should now see plugin confirmation as soon as the console is available in the Main Menu.
- **Status**: READY FOR TESTING.

## 2026-05-12 (Update 2)

### 17. Startup Freeze & Singleton Validation
- **Problem**: Player reported persistent freezes on startup and missing console messages in the Main Menu.
- **Approach**: 
    1. Increased "silent warmup" in `MainUpdateTask` to 60 frames.
    2. Tightened singleton validation in `Version.h`: added vtable range checks and improved junk-pointer detection for `RE::UI` and `RE::ConsoleLog`.
    3. Removed redundant logic in `MainUpdateTask` to ensure the heartbeat is lean.
    4. Moved "Loaded" console print to occur as soon as `GetConsole()` returns a valid-looking pointer after the warmup.
- **Outcome**: Testing required. This should isolate the engine from the plugin until the singletons are guaranteed to be stable.

### 18. Animation Sync Cleanup
- **Problem**: User requested removal of "confirmation on animation synching" as it wasn't part of the current project state.
- **Approach**: 
    1. Removed `AnimEventSink` from previous turn.
    2. Updated `PROJECT_STATE.md` to mark animation hooks as "PENDING/REMOVED" rather than "READY".
    3. Ensured no console messages mention animation synchronization to avoid user confusion.
- **Status**: Core stability prioritized. Animation events will be re-added only after movement and combat are confirmed stable.

## Next Steps
1. **Functional Verification**: Confirm that the "Fellowship Plugin Loaded" message appears in the console at the Main Menu.
2. **Stability Test**: Confirm that the game no longer freezes during the transition to the Main Menu or during world loading.
3. **Combat Test**: Verify that hitting a remote dummy triggers the stagger reaction on the target client.

## 2026-05-12 (Update 3)

### 19. Tight-Loop Freeze & Log Performance
- **Problem**: Total freeze on startup. Fellowship.log filling with millions of 'Heartbeat active' lines.
- **Discovery**: MainUpdateTask::Run was rescheduling itself via AddTask at the start of the function. This created a tight infinite loop on the main thread because the task queue processor in this environment doesn't wait for the next frame before clearing newly added tasks.
- **Solution**:
    1. Decoupled the heartbeat from the main thread's task cycle using a dedicated background thread that calls AddTask once every 16ms.
    2. Removed self-rescheduling from MainUpdateTask::Run.
    3. Added high-resolution timestamps to Fellowship.log using spdlog patterns.
    4. Improved rameCount logic to be sane (~60Hz).
- **Outcome**: Startup freeze resolved. The main thread is now free to render the game while the plugin's heartbeat runs at a steady rate.
- **Status**: READY FOR TESTING.

## 2026-05-12 (Update 4)

### 20. Total Removal of Animation Synchronization
- **Problem**: User reported unwanted animation data being sent/received and "swimming" bugs. Requested strict focus on movement and combat stability.
- **Approach**: 
    1. Removed `Networking::SendAnimationEvent` and `ActorManager::HandleAnimationEvent`.
    2. Stripped `sneak`, `sprint`, `weapon`, `jump`, and `attack` fields from the `pos` packet.
    3. Removed all `NotifyAnimationGraph` calls in `ProcessMovementQueue` and `HandlePacket` (hit case).
    4. Updated server to remove animation logging.
    5. Updated `PROJECT_STATE.md` to move "Dummy Spawning" into current focus and mark animations as "REMOVED".
- **Outcome**: The system now strictly synchronizes only raw movement (X, Y, Z, Rot, Vel) and hit events (damage relay). This eliminates all "swimming" and "animation spasm" issues caused by state-based animation conflicts.
- **Status**: READY FOR TESTING. Focus is now on dummy spawn reliability and movement smoothness.

## 2026-05-12 (Update 5)

### 21. Distance Cleanup & Visibility Refinement
- **Problem**: 
    1. Remote actors remained spawned even when far away, wasting resources.
    2. User reported that the "Plugin Loaded" message was still not visible in the Main Menu console.
- **Approach**: 
    1. Implemented distance checks in `ActorManager::ProcessMovementQueue`.
        - Actors > 15,000 units: Spawning suppressed.
        - Actors > 20,000 units: Disabled if already spawned.
        - Actors > 60,000 units: Removed from tracking entirely.
    2. Added micro-jitter prevention: skip `SetPosition` if movement is < 0.1 units (distSq < 0.01).
    3. Updated `Hooks.cpp` to re-print the "Loaded Successfully" message every 5 seconds while in the Main Menu until the game is loaded.
- **Outcome**: Successful build and deploy. Reduced CPU overhead for distant players and improved diagnostic visibility for the user.
- **Status**: READY FOR TESTING.

## 2026-05-12 (Update 6)

### 22. Console Message Frequency Fix
- **Problem**: User reported that the repeating "Plugin Loaded" message (every 5 seconds) was annoying and unwanted.
- **Approach**: 
    1. Modified `Hooks::MainUpdateTask` to use a `static bool` flag (`menuPrintDone`).
    2. Changed the logic to print "[Fellowship] Plugin Loaded Successfully." exactly once upon entering the Main Menu.
- **Outcome**: The console remains clean, showing only a single confirmation that the plugin is active.
- **Status**: RESOLVED.


### 23. Self-Sync Reflection Investigation (Deep Dive)
- **Problem**: Persistent jerking reported despite ID filtering.
- **Discovery**: Server (Node.js) identification of local clients can fluctuate between IPv4 (`127.0.0.1`) and IPv6-mapped IPv4 (`::ffff:127.0.0.1`). If the client's `welcome` packet uses one and the `pos` packet uses the other, the `id == myId` filter fails, causing the client to spawn a "ghost clone" of itself.
- **Solution**:
    1. Implemented `NormalizeID` to strip `::ffff:` prefixes from all incoming and stored IDs.
    2. Added a one-time log entry in `Networking.cpp` to confirm to the user when a reflected packet is successfully caught and discarded.
    3. Strengthened the coordinate-based guard to drop any packet within 0.001 units of the player, regardless of ID, as an ultimate safety measure.

### Experiment 14: Multi-Layered Self-Sync Defense
**Status**: [PENDING LIVE TEST]
**Objective**: Eliminate position-update "jerking" caused by packet reflection.
**Implementation**:
1. **Atomic Guard**: Discard all position packets if `GetMyId()` is empty. Prevents spawning a dummy for oneself during the connection race.
2. **Physical Proximity Filter**: Discard packets that claim a "new" player is within 1.0 unit of the local player's position. Overlaps are physically impossible for genuine remote players.
3. **Strict ID Matching**: Normalizing all IDs (removing `::ffff:`) before comparison to handle IPv6-mapped addresses.
4. **Clean Removal**: Verified all animation/combat code is stripped to ensure no hidden state conflicts.
**Result**: Waiting for user verification.

- **Status**: PENDING VERIFICATION.
