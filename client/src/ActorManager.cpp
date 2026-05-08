#include "ActorManager.h"
#include <SKSE/SKSE.h>
#include <mutex>

namespace Multiplayer {

    static std::mutex g_playerMutex;

    ActorManager& ActorManager::Get() {
        static ActorManager instance;
        return instance;
    }

    void ActorManager::UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot) {
        std::lock_guard<std::mutex> lock(g_playerMutex);
        
        auto it = m_remotePlayers.find(id);
        if (it == m_remotePlayers.end()) {
            m_remotePlayers[id] = {x, y, z, rot, nullptr, true};
        } else {
            it->second.x = x;
            it->second.y = y;
            it->second.z = z;
            it->second.rot = rot;
        }
    }

    RE::Actor* ActorManager::SpawnDummyActor(float x, float y, float z, float rot) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;

        // Use a generic actor base ID (e.g. EncBandit01Template - 0x0003E01E)
        auto base = RE::TESForm::LookupByID<RE::TESNPC>(0x0001CB8A); // A basic fox for debug, or use player base 0x7
        if (!base) return nullptr;

        auto ref = player->PlaceAtMe(base, 1, false, false);
        if (ref) {
            auto actor = ref->As<RE::Actor>();
            if (actor) {
                RE::NiPoint3 pos{x, y, z};
                actor->SetPosition(pos, true);
                actor->SetAngleZ(rot);
                return actor;
            }
        }
        return nullptr;
    }

    void ActorManager::ProcessMovementQueue() {
        std::lock_guard<std::mutex> lock(g_playerMutex);

        for (auto& [id, data] : m_remotePlayers) {
            if (data.needsSpawn) {
                data.actorRef = SpawnDummyActor(data.x, data.y, data.z, data.rot);
                if (data.actorRef) {
                    data.needsSpawn = false;
                    SKSE::log::info("Spawned actor for player {}", id);
                }
            } else if (data.actorRef) {
                // Update position
                RE::NiPoint3 pos{data.x, data.y, data.z};
                data.actorRef->SetPosition(pos, true);
                data.actorRef->SetAngleZ(data.rot);
            }
        }
    }
}
