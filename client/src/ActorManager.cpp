#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "ActorManager.h"
#include <mutex>

namespace Multiplayer {

    static std::mutex g_playerMutex;

    ActorManager& ActorManager::Get() {
        static ActorManager instance;
        return instance;
    }

    void ActorManager::UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot, uint32_t cellID, uint32_t worldID) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        
        auto it = m_remotePlayers.find(id);
        if (it == m_remotePlayers.end()) {
            m_remotePlayers[id] = {x, y, z, rot, x, y, z, rot, cellID, worldID, {}, true};
        } else {
            it->second.targetX = x;
            it->second.targetY = y;
            it->second.targetZ = z;
            it->second.targetRot = rot;
            it->second.cellID = cellID;
            it->second.worldID = worldID;
        }
    }

    void ActorManager::PlayRemoteAnimation(const std::string& id, const std::string& animEvent) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        
        auto it = m_remotePlayers.find(id);
        if (it != m_remotePlayers.end()) {
            it->second.pendingAnimations.push_back(animEvent);
        }
    }

    RE::ObjectRefHandle ActorManager::SpawnDummyActor(float x, float y, float z, float rot) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return {};

        // Use the player base ID (0x7) so animations match
        auto base = RE::TESForm::LookupByID<RE::TESNPC>(0x7); 
        if (!base) return {};

        auto ref = player->PlaceObjectAtMe(base, false);
        if (ref) {
            auto actor = ref->As<RE::Actor>();
            if (actor) {
                // Ensure actor is in the same cell/world as player initially
                actor->MoveTo(player);
                
                RE::NiPoint3 pos{x, y, z};
                actor->SetPosition(pos, true);
                actor->data.angle.z = rot;

                // Make sure they are visible and active
                actor->SetAlpha(1.0f);
                actor->Enable(false);
                
                SKSE::log::info("Successfully created dummy actor at {:08X}", actor->GetFormID());
                return actor->GetHandle();
            }
        }
        return {};
    }

    void ActorManager::ProcessMovementQueue() {
        std::lock_guard<std::mutex> lock(g_playerMutex);

        auto localPlayer = RE::PlayerCharacter::GetSingleton();
        if (!localPlayer) return;

        auto localCell = localPlayer->GetParentCell();
        auto localWorld = localPlayer->GetWorldspace();
        uint32_t localCellID = localCell ? localCell->GetFormID() : 0;
        uint32_t localWorldID = localWorld ? localWorld->GetFormID() : 0;

        for (auto& [id, data] : m_remotePlayers) {
            // Check if we need to spawn
            bool needsSpawn = data.needsSpawn;
            RE::Actor* actor = nullptr;

            if (data.actorHandle) {
                auto ref = data.actorHandle.get();
                if (ref) {
                    actor = ref->As<RE::Actor>();
                } else {
                    needsSpawn = true;
                }
            } else {
                needsSpawn = true;
            }

            if (needsSpawn) {
                data.actorHandle = SpawnDummyActor(data.targetX, data.targetY, data.targetZ, data.targetRot);
                if (data.actorHandle) {
                    data.needsSpawn = false;
                    data.x = data.targetX;
                    data.y = data.targetY;
                    data.z = data.targetZ;
                    data.rot = data.targetRot;
                    
                    SKSE::log::info("Spawned actor for player {}", id);
                    auto console = RE::ConsoleLog::GetSingleton();
                    if (console) console->Print("[Fellowship] Spawned remote player: %s", id.c_str());
                    
                    auto ref = data.actorHandle.get();
                    actor = ref ? ref->As<RE::Actor>() : nullptr;
                }
            }

            if (actor) {
                // Determine visibility
                bool sameWorld = (data.worldID != 0 && data.worldID == localWorldID);
                bool sameInterior = (data.worldID == 0 && localWorldID == 0 && data.cellID == localCellID);
                
                if (sameWorld || sameInterior || data.cellID == 0) {
                    if (actor->IsDisabled()) {
                        SKSE::log::info("Enabling actor for {} (Entering same area)", id);
                        actor->Enable(false);
                        // If they were disabled, they might be in a different cell, so move them to us first
                        actor->MoveTo(localPlayer); 
                    }
                    
                    // Interpolate position
                    float lerpFactor = 0.15f;
                    data.x += (data.targetX - data.x) * lerpFactor;
                    data.y += (data.targetY - data.y) * lerpFactor;
                    data.z += (data.targetZ - data.z) * lerpFactor;
                    data.rot += (data.targetRot - data.rot) * lerpFactor;

                    RE::NiPoint3 pos{data.x, data.y, data.z};
                    actor->SetPosition(pos, true);
                    actor->data.angle.z = data.rot;
                } else {
                    // Different cell/world - hide them
                    if (!actor->IsDisabled()) {
                        SKSE::log::info("Disabling actor for {} (Left area)", id);
                        actor->Disable();
                    }
                }

                // Play any pending animations
                for (const auto& anim : data.pendingAnimations) {
                    actor->NotifyAnimationGraph(anim);
                }
                data.pendingAnimations.clear();
            }
        }
    }

    void ActorManager::PrintSyncStatus() {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        auto console = RE::ConsoleLog::GetSingleton();
        if (!console) return;

        console->Print("---------------------------------------");
        console->Print("[Fellowship] Synchronization Status");
        
        auto player = RE::PlayerCharacter::GetSingleton();
        if (player) {
            auto pos = player->GetPosition();
            console->Print("Local Player: [%.1f, %.1f, %.1f]", pos.x, pos.y, pos.z);
            
            auto cell = player->GetParentCell();
            auto world = player->GetWorldspace();
            if (cell) {
                console->Print("Current Cell: %s (ID: %08X)", cell->GetFullName(), cell->GetFormID());
            }
            if (world) {
                console->Print("Current World: %s (ID: %08X)", world->GetFullName(), world->GetFormID());
            }
        }

        console->Print("Remote Players: %zu", m_remotePlayers.size());
        for (const auto& [id, data] : m_remotePlayers) {
            float dist = player ? player->GetPosition().GetDistance({data.x, data.y, data.z}) : 0.0f;
            const char* status = "Unknown";
            
            auto ref = data.actorHandle.get();
            auto actor = ref ? ref->As<RE::Actor>() : nullptr;
            
            if (actor) {
                status = actor->IsDisabled() ? "Hidden (Different Cell)" : "Visible";
            } else if (data.needsSpawn) {
                status = "Pending Spawn";
            } else {
                status = "Invalid Handle";
            }

            console->Print("  ID: %s", id.c_str());
            console->Print("  Pos: [%.1f, %.1f, %.1f] (Dist: %.1f)", data.x, data.y, data.z, dist);
            console->Print("  World/Cell: %08X / %08X", data.worldID, data.cellID);
            console->Print("  Status: %s", status);
        }
        console->Print("---------------------------------------");
    }
}
