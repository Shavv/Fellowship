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
            m_remotePlayers[id] = {x, y, z, rot, x, y, z, rot, cellID, worldID, nullptr, true};
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

    RE::Actor* ActorManager::SpawnDummyActor(float x, float y, float z, float rot) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;

        // Use the player base ID (0x7) so animations match
        auto base = RE::TESForm::LookupByID<RE::TESNPC>(0x7); 
        if (!base) return nullptr;

        auto ref = player->PlaceObjectAtMe(base, false);
        if (ref) {
            auto actor = ref->As<RE::Actor>();
            if (actor) {
                // Ensure actor is in the same cell/world as player
                actor->MoveTo(player);
                
                RE::NiPoint3 pos{x, y, z};
                actor->SetPosition(pos, true);
                actor->data.angle.z = rot;

                // Make sure they are visible and active
                actor->SetAlpha(1.0f);
                actor->Enable(false);
                
                return actor;
            }
        }
        return nullptr;
    }

    void ActorManager::ProcessMovementQueue() {
        std::lock_guard<std::mutex> lock(g_playerMutex);

        for (auto& [id, data] : m_remotePlayers) {
            if (data.needsSpawn) {
                data.actorRef = SpawnDummyActor(data.targetX, data.targetY, data.targetZ, data.targetRot);
                if (data.actorRef) {
                    data.needsSpawn = false;
                    data.x = data.targetX;
                    data.y = data.targetY;
                    data.z = data.targetZ;
                    data.rot = data.targetRot;
                    
                    SKSE::log::info("Spawned actor for player {}", id);
                    auto console = RE::ConsoleLog::GetSingleton();
                    if (console) console->Print("[Fellowship] Spawned remote player: %s", id.c_str());
                }
            } else if (data.actorRef) {
                auto localPlayer = RE::PlayerCharacter::GetSingleton();
                if (localPlayer) {
                    auto localCell = localPlayer->GetParentCell();
                    uint32_t localCellID = localCell ? localCell->GetFormID() : 0;
                    
                    // If remote player is in the same cell, update and show them
                    if (data.cellID == localCellID || data.cellID == 0) {
                        if (data.actorRef->IsDisabled()) {
                            data.actorRef->Enable(false);
                            data.actorRef->MoveTo(localPlayer); // Bring them back to our world
                        }
                        
                        // Interpolate position
                        float lerpFactor = 0.15f;
                        data.x += (data.targetX - data.x) * lerpFactor;
                        data.y += (data.targetY - data.y) * lerpFactor;
                        data.z += (data.targetZ - data.z) * lerpFactor;
                        data.rot += (data.targetRot - data.rot) * lerpFactor;

                        RE::NiPoint3 pos{data.x, data.y, data.z};
                        data.actorRef->SetPosition(pos, true);
                        data.actorRef->data.angle.z = data.rot;
                    } else {
                        // Different cell - hide them
                        if (!data.actorRef->IsDisabled()) {
                            data.actorRef->Disable();
                        }
                    }
                }

                // Play any pending animations
                for (const auto& anim : data.pendingAnimations) {
                    data.actorRef->NotifyAnimationGraph(anim);
                }
                data.pendingAnimations.clear();
            }
        }
    }
}
