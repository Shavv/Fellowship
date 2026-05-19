#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <RE/B/bhkCharacterController.h>
#include <RE/H/hkVector4.h>
#include "ActorManager.h"
#include "Version.h"
#include "Networking.h"
#include <mutex>

namespace Multiplayer {

    static std::mutex g_playerMutex;

    ActorManager& ActorManager::Get() {
        static ActorManager instance;
        return instance;
    }

    void ActorManager::UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot, float vx, float vy, float vz, uint32_t cellID, uint32_t worldID) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        
        std::string myId = Networking::Get().GetMyId();
        if (id.empty() || id == "unknown" || (!myId.empty() && id == myId)) {
            // If it's us, make sure we aren't in the map
            auto it = m_remotePlayers.find(id);
            if (it != m_remotePlayers.end()) {
                if (it->second.actorHandle) {
                    auto ref = it->second.actorHandle.get();
                    if (ref) ref->Disable();
                }
                m_remotePlayers.erase(it);
            }
            return;
        }

        auto it = m_remotePlayers.find(id);
        if (it == m_remotePlayers.end()) {
            SKSE::log::info("ActorManager: New remote player detected: {}. Initializing state...", id);
            PlayerData newData;
            newData.x = x; newData.targetX = x;
            newData.y = y; newData.targetY = y;
            newData.z = z; newData.targetZ = z;
            newData.rot = rot; newData.targetRot = rot;
            newData.vx = vx; newData.vy = vy; newData.vz = vz;
            newData.cellID = cellID;
            newData.worldID = worldID;
            m_remotePlayers[id] = newData;
        } else {
            it->second.targetX = x;
            it->second.targetY = y;
            it->second.targetZ = z;
            it->second.targetRot = rot;
            it->second.vx = vx;
            it->second.vy = vy;
            it->second.vz = vz;
            it->second.cellID = cellID;
            it->second.worldID = worldID;
        }
    }

    void ActorManager::RemovePlayer(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        auto it = m_remotePlayers.find(id);
        if (it != m_remotePlayers.end()) {
            if (it->second.actorHandle) {
                auto ref = it->second.actorHandle.get();
                if (ref) ref->Disable();
            }
            m_remotePlayers.erase(it);
            SKSE::log::info("ActorManager: Explicitly removed player {}", id);
        }
    }

    bool ActorManager::IsTracking(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        return m_remotePlayers.find(id) != m_remotePlayers.end();
    }



    std::string ActorManager::GetIdFromHandle(RE::ObjectRefHandle handle) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        for (auto const& [id, data] : m_remotePlayers) {
            if (data.actorHandle == handle) {
                return id;
            }
        }
        return "";
    }

    RE::ObjectRefHandle ActorManager::SpawnDummyActor(float x, float y, float z, float rot) {
        auto player = Fellowship::GetPlayer();
        if (!player || (uintptr_t)player < 0x1000 || (uintptr_t)player == 0x6e6576456563616c) {
            SKSE::log::error("ActorManager: Cannot spawn dummy - Local player is invalid!");
            return {};
        }

        // Use a generic Prisoner NPC base (0x10967) instead of the Player base (0x7) to prevent state crossover
        auto base = RE::TESForm::LookupByID<RE::TESNPC>(0x10967); 
        if (!base) {
            SKSE::log::error("ActorManager: Failed to lookup generic NPC (0x10967)!");
            return {};
        }

        SKSE::log::info("ActorManager: Spawning dummy actor at ({:.1f}, {:.1f}, {:.1f})...", x, y, z);
        auto ref = player->PlaceObjectAtMe(base, false);
        if (ref) {
            auto actor = ref->As<RE::Actor>();
            if (actor) {
                // Set initial position and rotation
                RE::NiPoint3 pos{x, y, z};
                actor->SetPosition(pos, true);
                actor->data.angle.z = rot;

                // FREEZE: Disable AI but allow position updates and animations to play
                actor->SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
                actor->StopCombat();
                actor->EnableAI(false);
                
                // MULTIPLAYER SAFETY: Rely on character controller flags instead of completely disabling root collision, which can detach havok
                // actor->SetCollision(false);
                
                actor->SetAlpha(1.0f);
                actor->Enable(false);
                
                // Suppress physics/velocity to prevent "walking" animations and completely disable controller collisions
                auto controller = actor->GetCharController();
                if (controller) {
                    controller->SetLinearVelocityImpl({0.0f, 0.0f, 0.0f, 0.0f});
                    controller->flags.set(RE::CHARACTER_FLAGS::kNoCharacterCollisions);
                    controller->flags.set(RE::CHARACTER_FLAGS::kNotPushablePermanent);
                    controller->gravity = 0.0f;
                }

                SKSE::log::info("ActorManager: Successfully spawned dummy actor. FormID: {:08X}", actor->GetFormID());
                return actor->GetHandle();
            }
        }
        return {};
    }

    void ActorManager::ProcessMovementQueue() {
        std::lock_guard<std::mutex> lock(g_playerMutex);

        auto localPlayer = Fellowship::GetPlayer();
        if (!localPlayer) return;

        auto localCell = localPlayer->GetParentCell();
        auto localWorld = localPlayer->GetWorldspace();
        uint32_t localCellID = localCell ? localCell->GetFormID() : 0;
        uint32_t localWorldID = localWorld ? localWorld->GetFormID() : 0;
        std::string myId = Networking::Get().GetMyId();

        for (auto it = m_remotePlayers.begin(); it != m_remotePlayers.end(); ) {
            const std::string& id = it->first;
            auto& data = it->second;

            if (!myId.empty() && id == myId) {
                if (data.actorHandle) {
                    auto ref = data.actorHandle.get();
                    if (ref) ref->Disable();
                }
                it = m_remotePlayers.erase(it);
                continue;
            }

            // 1. Distance-based logic
            RE::NiPoint3 localPos = localPlayer->GetPosition();
            float distToPlayerSq = (data.targetX - localPos.x) * (data.targetX - localPos.x) +
                                   (data.targetY - localPos.y) * (data.targetY - localPos.y) +
                                   (data.targetZ - localPos.z) * (data.targetZ - localPos.z);

            // PERSISTENT PROXIMITY FILTER: 
            // Discard any packet from a player who is overlapping us UNLESS we have seen them move.
            // If a player appears for the first time INSIDE our model, it is a Ghost.
            if (distToPlayerSq < 100.0f && !data.actorHandle) {
                SKSE::log::info("ActorManager: Discarding ghost actor {} (Proximity filter)", id);
                it = m_remotePlayers.erase(it);
                continue;
            }

            // If extremely far (approx 5-6 cells), remove from tracking to save memory
            if (distToPlayerSq > 60000.0f * 60000.0f) {
                if (data.actorHandle) {
                    auto ref = data.actorHandle.get();
                    if (ref) ref->Disable();
                }
                SKSE::log::info("ActorManager: Removing distant player {} from tracking.", id);
                it = m_remotePlayers.erase(it);
                continue;
            }

            RE::Actor* actor = nullptr;

            if (data.actorHandle) {
                auto ref = data.actorHandle.get();
                if (ref) {
                    actor = ref->As<RE::Actor>();
                } else {
                    data.needsSpawn = true;
                }
            } else {
                data.needsSpawn = true;
            }

            if (data.needsSpawn) {
                // Only spawn if within reasonable range (approx 3 cells)
                if (distToPlayerSq < 15000.0f * 15000.0f) {
                    data.actorHandle = SpawnDummyActor(data.targetX, data.targetY, data.targetZ, data.targetRot);
                    if (data.actorHandle) {
                        data.needsSpawn = false;
                        data.x = data.targetX;
                        data.y = data.targetY;
                        data.z = data.targetZ;
                        data.rot = data.targetRot;
                        
                        SKSE::log::info("Spawned actor for player {}", id);
                        Networking::Get().SendSpawnLog(id);
                        auto ref = data.actorHandle.get();
                        actor = ref ? ref->As<RE::Actor>() : nullptr;
                    }
                }
            }

            if (actor) {
                // FATAL SAFETY: Ensure we never, ever touch the local player in this loop.
                if (actor == localPlayer || actor->GetFormID() == 0x14) {
                    SKSE::log::error("ActorManager: [CRITICAL] Remote player loop is attempting to update the local player! ID: {}", id);
                    it = m_remotePlayers.erase(it);
                    continue;
                }

                // Determine visibility (Cell/World check)
                bool sameWorld = (data.worldID != 0 && data.worldID == localWorldID);
                bool sameInterior = (data.worldID == 0 && localWorldID == 0 && data.cellID == localCellID);
                bool inRange = distToPlayerSq < 20000.0f * 20000.0f; // Approx 5 cells
                
                if ((sameWorld || sameInterior || data.cellID == 0) && inRange) {
                    // VISUAL EXCLUSION ZONE with HYSTERESIS: 
                    // To prevent rapid Enable/Disable toggling that causes massive physics lag spikes (which leads to the player clipping into bridges and swimming),
                    // we disable if within 50 units, but only re-enable if they are further than 100 units away.
                    if (distToPlayerSq < 50.0f * 50.0f) {
                        if (!actor->IsDisabled()) actor->Disable();
                    } else if (distToPlayerSq > 100.0f * 100.0f) {
                        if (actor->IsDisabled()) actor->Enable(false);
                    }
                    
                    // Interpolate position smoothly (Frame-independent LERP)
                    float dt = RE::BSTimer::GetSingleton()->delta;
                    
                    float distSq = (data.targetX - data.x) * (data.targetX - data.x) + 
                                   (data.targetY - data.y) * (data.targetY - data.y) + 
                                   (data.targetZ - data.z) * (data.targetZ - data.z);

                    // Micro-jitter prevention: don't update if change is negligible (< 0.1 units)
                    if (distSq > 0.01f) {
                        if (distSq > 1000000.0f) { // Teleport if > 1000 units
                            data.x = data.targetX;
                            data.y = data.targetY;
                            data.z = data.targetZ;
                            data.rot = data.targetRot;
                        } else {
                            // Use a consistent LERP speed (10.0f is smoother for 30Hz packets)
                            float lerpFactor = 10.0f * dt;
                            if (lerpFactor > 1.0f) lerpFactor = 1.0f;

                            data.x += (data.targetX - data.x) * lerpFactor;
                            data.y += (data.targetY - data.y) * lerpFactor;
                            data.z += (data.targetZ - data.z) * lerpFactor;
                            
                            // Rotation LERP (shortest path)
                            float rotDiff = data.targetRot - data.rot;
                            while (rotDiff > 3.141592f) rotDiff -= 6.283185f;
                            while (rotDiff < -3.141592f) rotDiff += 6.283185f;
                            data.rot += rotDiff * (lerpFactor * 1.5f);
                        }

                        RE::NiPoint3 pos{data.x, data.y, data.z};
                        actor->SetPosition(pos, true);
                        actor->data.angle.z = data.rot;
                    }
                    
                    auto controller = actor->GetCharController();
                    if (controller) {
                        // Maintain custom physics settings to prevent local player swimming issues
                        controller->flags.set(RE::CHARACTER_FLAGS::kNoCharacterCollisions);
                        controller->flags.set(RE::CHARACTER_FLAGS::kNotPushablePermanent);
                        controller->gravity = 0.0f;

                        RE::hkVector4 velocity(data.vx, data.vy, data.vz, 0.0f);
                        controller->SetLinearVelocityImpl(velocity);
                    }
                } else {
                    if (!actor->IsDisabled()) {
                        actor->Disable();
                    }
                }
            }
            ++it;
        }
    }

    void ActorManager::CleanupOrphans() {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        auto processLists = RE::ProcessLists::GetSingleton();
        if (!processLists) return;

        for (auto& handle : processLists->highActorHandles) {
            auto ref = handle.get();
            if (!ref) continue;
            auto actor = ref->As<RE::Actor>();
            if (!actor) continue;
            
            // Skip the local player
            if (actor->GetFormID() == 0x14) continue;
            
            auto base = actor->GetActorBase();
            if (base && base->GetFormID() == 0x10967) {
                // It's a prisoner NPC. Are we tracking it?
                bool isTracked = false;
                for (const auto& [id, data] : m_remotePlayers) {
                    if (data.actorHandle == handle) {
                        isTracked = true;
                        break;
                    }
                }
                
                if (!isTracked) {
                    SKSE::log::info("ActorManager: Cleaning up orphaned dummy actor {:08X}", actor->GetFormID());
                    actor->Disable();
                    actor->SetDelete(true);
                }
            }
        }
    }
}
