#pragma once
#include <enet/enet.h>
#include <thread>
#include <atomic>
#include <string>

namespace Multiplayer {
    class Networking {
    public:
        static Networking& Get();

        bool Initialize();
        void Connect(const std::string& host, uint16_t port);
        void Disconnect();
        
        // Call this from a game hook to broadcast our position
        void SendPositionUpdate(float x, float y, float z, float rot);

    private:
        Networking() = default;
        ~Networking();

        void NetworkLoop();

        ENetHost* m_client{nullptr};
        ENetPeer* m_peer{nullptr};
        std::thread m_networkThread;
        std::atomic<bool> m_running{false};
    };
}
