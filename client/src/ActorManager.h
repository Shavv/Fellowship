#pragma once
#include <string>
#include <unordered_map>
#include <RE/Skyrim.h>

namespace Multiplayer {
    class ActorManager {
    public:
        static ActorManager& Get();

        // Called when network data is received
        void UpdateRemotePlayer(const std::string& id, float x, float y, float z, float rot);
        
        // Called every frame to actually apply movements (Skyrim engine isn't thread safe, 
        // so network thread queues data and game thread applies it)
        void ProcessMovementQueue();

    private:
        ActorManager() = default;
        
        struct PlayerData {
            float x, y, z, rot;
            RE::Actor* actorRef{nullptr};
            bool needsSpawn{true};
        };

        std::unordered_map<std::string, PlayerData> m_remotePlayers;
        
        // Spawns a basic dummy actor
        RE::Actor* SpawnDummyActor(float x, float y, float z, float rot);
    };
}
