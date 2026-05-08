#include "Hooks.h"
#include "Networking.h"
#include "ActorManager.h"
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

namespace Multiplayer {
    
    // We hook the main update loop (Main::Update)
    class MainUpdateHook {
    public:
        static void Install() {
            REL::Relocation<uintptr_t> updateAddr{ REL::RelocationID(35551, 36544) }; 
            // Note: Address IDs depend on AE/SSE version, 35551 is a common Main::Update in SSE.
            // Using modern SKSE trampoline to hook:
            auto& trampoline = SKSE::GetTrampoline();
            _Update = trampoline.write_call<5>(updateAddr.address() + 0x11F, Update);
            SKSE::log::info("Installed Main::Update Hook");
        }

    private:
        static void Update(RE::Main* a_this, float a2) {
            _Update(a_this, a2); // Call original

            // Process remote player movements on the game thread
            ActorManager::Get().ProcessMovementQueue();

            // Read player data
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player && player->Is3DLoaded()) {
                auto pos = player->GetPosition();
                auto rot = player->GetAngleZ(); // Just Z-rotation for simplicity
                
                // Throttle updates (e.g. 20 ticks per second instead of per-frame) to save bandwidth
                static auto lastTick = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() > 50) {
                    Networking::Get().SendPositionUpdate(pos.x, pos.y, pos.z, rot);
                    lastTick = now;
                }
            }
        }

        static inline REL::Relocation<decltype(Update)> _Update;
    };

    void Hooks::Install() {
        SKSE::AllocTrampoline(64);
        MainUpdateHook::Install();
    }
}
