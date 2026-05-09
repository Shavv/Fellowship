#pragma once
#include <string>
#include <unordered_map>
#include <RE/Skyrim.h>

namespace Multiplayer {
    class ActorManager {
    public:
        static ActorManager& Get();

        // Called when network data is received
        void UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot, uint32_t cellID, uint32_t worldID);
        
        // Called when network data receives an animation event
        void PlayRemoteAnimation(const std::string& id, const std::string& animEvent);
        
        // Called every frame to actually apply movements (Skyrim engine isn't thread safe, 
        // so network thread queues data and game thread applies it)
        void ProcessMovementQueue();

    private:
        ActorManager() = default;
        
        struct PlayerData {
            float x, y, z, rot;
            float targetX, targetY, targetZ, targetRot;
            uint32_t cellID, worldID;
            RE::Actor* actorRef{nullptr};
            bool needsSpawn{true};
            std::vector<std::string> pendingAnimations;
        };

        std::unordered_map<std::string, PlayerData> m_remotePlayers;
        
        // Spawns a basic dummy actor
        RE::Actor* SpawnDummyActor(float x, float y, float z, float rot);
    };
}
