#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
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

    // We hook PlayerCharacter::Update via VTable
    class PlayerUpdateHook {
    public:
        static void Install() {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return;

            // VTable hook for Update (index 0xAD / 173)
            REL::Relocation<uintptr_t> vtable{ RE::VTABLE_PlayerCharacter[0] };
            _Update = vtable.write_vfunc(0xAD, Update);
            
            SKSE::log::info("Installed PlayerCharacter::Update VTable Hook");
        }

    private:
        using UpdateFn = void(RE::PlayerCharacter*, float);
        static void Update(RE::PlayerCharacter* a_this, float a_delta) {
            _Update(a_this, a_delta); // Call original function

            // Only run if this is the local player
            if (a_this != RE::PlayerCharacter::GetSingleton()) return;

            // Process remote player movements
            ActorManager::Get().ProcessMovementQueue();

            if (a_this->Is3DLoaded()) {
                static bool sinkRegistered = false;
                if (!sinkRegistered) {
                    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
                    if (a_this->GetAnimationGraphManager(manager)) {
                        if (manager && !manager->graphs.empty() && manager->graphs[0]) {
                            manager->graphs[0]->AddEventSink(AnimationEventSink::GetSingleton());
                            sinkRegistered = true;
                            SKSE::log::info("Registered Animation Event Sink");
                        }
                    }
                }

                auto pos = a_this->GetPosition();
                auto rot = a_this->GetAngleZ();
                
                static auto lastTick = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() > 50) {
                    Networking::Get().SendPositionUpdate(pos.x, pos.y, pos.z, rot);
                    lastTick = now;
                }
            }
        }

        static inline REL::Relocation<UpdateFn> _Update;
    };

    class MainUpdateHook {
    public:
        static void Install() {
            // Main VTable hook for Update (index 1)
            REL::Relocation<uintptr_t> vtable{ RE::VTABLE_Main[0] };
            _Update = vtable.write_vfunc(0x1, Update);
            SKSE::log::info("Installed Main::Update Hook");
        }

    private:
        using UpdateFn = void(RE::Main*);
        static void Update(RE::Main* a_this) {
            _Update(a_this);
            
            static int frameCount = 0;
            if (++frameCount % 300 == 0) {
                SKSE::log::info("Main Heartbeat... needsCOC={}", Networking::Get().NeedsCOC());
            }

            Networking::Get().ProcessNotifications();
            
            static bool playerHooked = false;
            if (!playerHooked) {
                if (RE::PlayerCharacter::GetSingleton()) {
                    PlayerUpdateHook::Install();
                    playerHooked = true;
                }
            }
        }

        static inline REL::Relocation<UpdateFn> _Update;
    };

    class MenuUpdateHook {
    public:
        static void Install() {
            // IMenu VTable hook for AdvanceMovie (index 5)
            REL::Relocation<uintptr_t> vtable{ RE::VTABLE_IMenu[0] };
            _AdvanceMovie = vtable.write_vfunc(0x5, AdvanceMovie);
            SKSE::log::info("Installed IMenu::AdvanceMovie Hook");
        }

    private:
        using AdvanceMovieFn = void(RE::IMenu*, float, std::uint32_t);
        static void AdvanceMovie(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
            _AdvanceMovie(a_this, a_interval, a_currentTime);
        }

        static inline REL::Relocation<AdvanceMovieFn> _AdvanceMovie;
    };

    void Hooks::Install() {
        MainUpdateHook::Install();
        MenuUpdateHook::Install();
    }
}
