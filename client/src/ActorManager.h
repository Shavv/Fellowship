#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <RE/Skyrim.h>

namespace Multiplayer {
    class ActorManager {
    public:
        static ActorManager& Get();

        void UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot, float vx, float vy, float vz, uint32_t cellID, uint32_t worldID);
        void RemovePlayer(const std::string& id);
        bool IsTracking(const std::string& id);

        std::string GetIdFromHandle(RE::ObjectRefHandle handle);
        void ProcessMovementQueue();
        void CleanupOrphans();

    private:
        ActorManager() = default;
        
        struct PlayerData {
            float x, y, z, rot;
            float targetX, targetY, targetZ, targetRot;
            float vx{0}, vy{0}, vz{0};
            uint32_t cellID, worldID;
            RE::ObjectRefHandle actorHandle;
            bool needsSpawn{true};
        };

        std::unordered_map<std::string, PlayerData> m_remotePlayers;
        RE::ObjectRefHandle SpawnDummyActor(float x, float y, float z, float rot);
    };
}
