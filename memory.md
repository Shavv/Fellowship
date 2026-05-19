# Fellowship Memory / Progress Notes

## Current State (2026-05-19)
- **Self-Sync Jerking**: Implemented Atomic ID Guard, Physical Proximity Filter, and Strict Normalization. (Pending live test results).
- **Distance-Based Cleanup**: Actors > 15k units are disabled; > 60k units are removed from tracking.
- **Micro-Jitter Prevention**: Added threshold check to prevent position updates for negligible movements (< 0.1 units).
- **Startup Stability / Connection**: Plugin automatically connects on startup and avoids infinite heartbeat loop. Console messages added.
- **Update Task Implementation**: Hooked `RE::PlayerCharacter::Update` (VTable index `0xAD` on `RE::VTABLE_PlayerCharacter[0]`) to safely handle local coordinate syncing at ~30Hz, eliminating loading screen hangs and crashes.
- **Main Menu Console Print**: Registered a `MenuOpenCloseEvent` sink on `kDataLoaded` that captures when `RE::MainMenu::MENU_NAME` opens and safely prints `[Fellowship] Plugin Loaded Successfully.` exactly once to the console.

## Problems Encountered & Solutions
- **Startup Freezes**: 
  - *Problem*: Infinite loop in heartbeat, engine singleton access during Main Menu or loading screens.
  - *Solution*: Decoupled heartbeat from SKSE task queue recursion. Added strict state checks to avoid executing game logic in menus/loading screens.
- **Address Library ID resolution failure**: 
  - *Problem*: Plugin failed to resolve IDs on 1.6.1170 game version.
  - *Solution*: Implemented version-aware runtime detection and ID lookup mechanism in the plugin's load sequence.
- **Self-Sync Jerking / Ghost Actors**: 
  - *Problem*: Local player's own position updates being echoed back, or IPv6-mapped addresses causing ID mismatch.
  - *Solution*: Enforced strict ID normalization and physical proximity filtering to discard local echo packets.
- **Main Client Swimming on Bridges**:
  - *Problem*: Local player was forced into swimming animations on bridges. Caused by dummy remote actors falling through geometry due to disabled collision and hitting water below, which triggered a global swim state.
  - *Root Cause*: The actor's character controller is `nullptr` during the initial `SpawnDummyActor` call because the actor's 3D hasn't finished loading. When the 3D loads later, the engine creates the controller with default settings, causing the dummy to fall and hit water.
  - *Solution*: Continuously set the dummy actor's `bhkCharacterController` `waterHeight` to `-30000.0f` and `gravity` to `0.0f` in the frame-by-frame `ProcessMovementQueue` loop as soon as the controller is available. This permanently prevents the dummy from entering a water state.
- **Client Swimming on Land / Save Corruption & Normal Land Sentinel**:
  - *Problem*: Even on brand new games, the local player would float or try to swim above land.
  - *Root Cause*: The physics repair routine we implemented mistakenly flagged `-30000.0f` (the standard Havok sentinel value for waterHeight on dry land) as a corrupted state. Because of this, the plugin constantly forced `waterHeight` to `-999999.0f` and reset swimming flags on every single frame. This infinite loop/tug-of-war with the engine's physics engine completely broke Havok's sensors and forced the character into swimming animations on dry land.
  - *Solution*: Updated `Hooks.cpp` to only treat `gravity == 0.0f` as a corrupted state. The dry-land sentinel water height (-30000.0f) is left completely untouched unless the player is actively stuck in a swimming state on land (`swimmingOnLand` is true), which cleanly and safely recovers the player.
  - *Addendum / Graphics Glitch*: Setting `waterHeight` to `-999999.0f` in a previous attempt completely broke the engine's water math, causing massive black rectangle rendering glitches. Reverted to using `-30000.0f` for both the local player repair and the dummy actors.

- **Ghosting / Self-Sync Jerking (Server IPv6 Bug)**:
  - *Problem*: The user noticed their position was getting updated by itself (ghosting) despite client-side filtering.
  - *Root Cause*: The NodeJS server's "One-Player-Per-IP" kicker logic failed to identify returning ghost connections. It used `key.split(':')[0]`, which fails on IPv6 mapped IPs (e.g. `::ffff:127.0.0.1`) because splitting by `:` returns an empty string for the first element, causing `oldIp === ip` to always fail. The server ended up assigning the player a new ID while keeping their old ghost session alive and broadcasting their old position.
  - *Solution*: Updated `server/index.js` to correctly normalize `::ffff:` IPs and use `lastIndexOf(':')` to strip only the port, successfully identifying and kicking old ghost connections for returning players.

- **Save Reloading Infinite Black Screen Freeze**:
  - *Problem*: During a loading screen, `MainUpdateTask` executed logic when the cell/world was still loading or fading in, occasionally calling engine features that were unsafe during pauses/fades.
  - *Solution*: Added checks for `RE::FaderMenu::MENU_NAME` and `ui->GameIsPaused()` to prevent any sync logic or actor spawning from running while the game is fully paused or transitioning. Also moved `Hooks::Install()` to `kDataLoaded` so background thread runs early.

- **Main Menu Console Print Missing**:
  - *Problem*: The `kDataLoaded` print event occurs before the UI and console are initialized.
  - *Solution*: Deferred the console print. The background thread waits until `ui->IsMenuOpen(RE::MainMenu::MENU_NAME)` and then triggers `Fellowship::PrintToConsole()`.

- **Client Swimming & Black Rectangle Graphical Glitches**:
  - *Problem*: The user reported the local player was swimming above land everywhere, and looking at water caused massive black rectangles on the screen.
  - *Root Cause*: We were calling `actor->SetPosition(pos, false)` on the dummy actors. The `false` parameter instructs the engine to skip updating Havok. This completely detaches the visual proxy from the Havok physics proxy. Moving the visual proxy rapidly without moving its associated Havok bounding box causes engine corruption, stretching the bounding box across the cell. If the dummy's bounding box intersected water, the water plane physics stretched across the entire map, triggering the local player's swimming state. The black boxes were visual artifacts from the broken physics bounding box intersecting the camera.
  - *Solution*: Reverted `SetPosition(pos, false)` to `SetPosition(pos, true)` so the Havok proxy correctly tracks the dummy actor. Additionally, removed the explicit `actor->SetCollision(false)` override to prevent completely detaching the proxy, relying instead on the `kNoCharacterCollisions` controller flag to prevent dummy actors from pushing the local player.

## Next Steps
- Perform live testing on dry land and in water to verify that player transitions naturally between walking and swimming states.
- Monitor console and logs to ensure the physics repair routine is silent during normal dry-land walking.
- Verify remote dummy synchronization movement.
