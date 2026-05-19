#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <RE/B/bhkCharacterController.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "Hooks.h"
#include "Networking.h"
#include "ActorManager.h"
#include "Version.h"

namespace Multiplayer {
    
    static bool g_gameLoaded = false;
    static uint32_t g_frameCount = 0;

    class PlayerUpdateHook {
    public:
        static void Install() {
            REL::Relocation<uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
            _Update = vtable.write_vfunc(0xAD, Update);
            SKSE::log::info("PlayerUpdateHook: Installed VTable hook at index 0xAD.");
        }

    private:
        static void Update(RE::PlayerCharacter* a_this, float a_delta) {
            _Update(a_this, a_delta);

            g_frameCount++;

            // 1. Silent Warmup
            if (g_frameCount < 60) {
                return;
            }

            // 2. Logging heartbeat & Orphan cleanup
            if (g_frameCount % 300 == 0) {
                ActorManager::Get().CleanupOrphans();
            }

            // 3. UI and Menu check
            auto ui = Fellowship::GetUI();
            if (!ui) {
                g_gameLoaded = false;
                return;
            }

            bool inMenu = ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || 
                          ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || 
                          ui->IsMenuOpen(RE::FaderMenu::MENU_NAME) || 
                          ui->GameIsPaused();

            // Don't proceed to player checks if in a menu or loading
            if (inMenu) {
                g_gameLoaded = false;
                return;
            }

            // Now we are NOT in a menu, so game should be in the active world.
            if (!a_this || !a_this->Is3DLoaded() || !a_this->GetParentCell()) {
                g_gameLoaded = false;
                return;
            }

            // Removed physics repair routine as it was corrupting the local player's physics state

            // Game has fully loaded and stabilized in the world
            if (!g_gameLoaded) {
                g_gameLoaded = true;
                SKSE::log::info("Fellowship: Player in world. Game is now loaded.");
                RE::DebugNotification("Fellowship Plugin Loaded Successfully.");
                Fellowship::PrintToConsole("Fellowship Plugin Loaded Successfully.");
                Fellowship::PrintToConsole("Fellowship: Synchronization loop starting.");
            }

            // 4. Core Synchronization Loop
            try {
                Networking::Get().ProcessNotifications();
                ActorManager::Get().ProcessMovementQueue();

                static auto lastTick = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() > 33) {
                    auto pos = a_this->GetPosition();
                    auto rot = a_this->GetAngleZ();
                    Networking::Get().SendPositionUpdate(pos.x, pos.y, pos.z, rot);
                    lastTick = now;
                }
            } catch (const std::exception& e) {
                SKSE::log::error("PlayerUpdateHook: Exception in sync loop: {}", e.what());
            } catch (...) {
                SKSE::log::error("PlayerUpdateHook: Unknown exception in sync loop.");
            }
        }

        static inline REL::Relocation<decltype(Update)> _Update;
    };

    void Hooks::Install() {
        static bool installed = false;
        if (installed) return;

        SKSE::log::info("Fellowship: Hooks::Install() - Initializing hooks.");
        PlayerUpdateHook::Install();
        installed = true;
    }

    void Hooks::NotifyGameLoaded() {
        // This can still be kept if needed from SKSE message handler, but the loop handles it robustly now.
        if (!g_gameLoaded) {
            SKSE::log::info("Fellowship: Game loaded signal received.");
        }
    }
}
