#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>

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

namespace Multiplayer {
    
    class AnimationEventSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimationEventSink* GetSingleton() {
            static AnimationEventSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
            if (!a_event || !a_event->holder) return RE::BSEventNotifyControl::kContinue;

            auto player = RE::PlayerCharacter::GetSingleton();
            if (a_event->holder == player) {
                // Sent by the local player!
                std::string animEvent(a_event->tag.c_str());
                Networking::Get().SendAnimationUpdate(animEvent);
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };

    // MainUpdateTask handles the periodic updates for the local player
    class MainUpdateTask {
    public:
        static void Run() {
            auto player = RE::PlayerCharacter::GetSingleton();
            
            // Check if player is valid and loaded
            // We use a safe range check and exclude the known 'junk' pointer 0x6e6576456563616c
            if (player && (uintptr_t)player > 0x1000 && (uintptr_t)player < 0x00007FFFFFFFFFFF && (uintptr_t)player != 0x6e6576456563616c && player->Is3DLoaded()) {
                // Process remote player movements
                ActorManager::Get().ProcessMovementQueue();

                // Periodic status logging
                static uint32_t tick = 0;
                if (++tick % 600 == 0) {
                    SKSE::log::info("MainUpdateTask: Syncing player at {:p}", (void*)player);
                    ActorManager::Get().PrintSyncStatus();
                }

                // Register animation sink if not done yet
                static bool sinkRegistered = false;
                if (!sinkRegistered) {
                    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
                    if (player->GetAnimationGraphManager(manager)) {
                        if (manager && !manager->graphs.empty() && manager->graphs[0]) {
                            manager->graphs[0]->AddEventSink(AnimationEventSink::GetSingleton());
                            sinkRegistered = true;
                            SKSE::log::info("MainUpdateTask: Registered Animation Event Sink");
                        }
                    }
                }

                // Send position update every ~50ms
                auto pos = player->GetPosition();
                auto rot = player->GetAngleZ();
                
                static auto lastTick = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() > 50) {
                    Networking::Get().SendPositionUpdate(pos.x, pos.y, pos.z, rot);
                    lastTick = now;
                }
            }

            // Manual trigger with F4
            static bool f4Down = false;
            if (GetAsyncKeyState(VK_F4) & 0x8000) {
                if (!f4Down) {
                    ActorManager::Get().PrintSyncStatus();
                    f4Down = true;
                }
            } else {
                f4Down = false;
            }

            Networking::Get().ProcessNotifications();

            // Re-queue for the next frame
            SKSE::GetTaskInterface()->AddTask(MainUpdateTask::Run);
        }
    };

    void Hooks::Install() {
        // Start the main update loop via TaskInterface
        // This loop handles both UI/Manual updates and Player synchronization
        SKSE::GetTaskInterface()->AddTask(MainUpdateTask::Run);
        SKSE::log::info("Fellowship: Main Update Task installed.");
    }
}
