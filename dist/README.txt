Skyrim Multiplayer Mod - Fellowship Test Build
-------------------------------------------

Installation Instructions:
-------------------------
1. Ensure you have SKSE64 and 'Address Library for SKSE Plugins' installed.
2. Ensure you have the Visual C++ 2015-2022 Redistributable (x64) installed.

Mod Files:
----------
A) Copy 'Fellowship.dll' to:
   [Skyrim Folder]/Data/SKSE/Plugins/

B) Copy 'fmt.dll' AND 'spdlog.dll' to:
   [Skyrim Folder]/  (The root folder containing SkyrimSE.exe)

Usage:
------
- The mod is currently hardcoded to connect to 192.168.178.123:3000.
- Make sure the host PC is running the server (node index.js).
- Animations and positions will sync automatically.

Troubleshooting:
----------------
- If you get error 0000007E: Ensure fmt.dll and spdlog.dll are in the ROOT folder, not the Plugins folder.
- IMPORTANT: On the Host PC, you MUST open UDP Port 3000 in Windows Firewall to allow other players to connect.
- Check the Server console window: it should say "Clients: 2" when both players are connected.
