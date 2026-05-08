#include "Networking.h"
#include <SKSE/SKSE.h>

namespace Multiplayer {
    Networking& Networking::Get() {
        static Networking instance;
        return instance;
    }

    Networking::~Networking() {
        Disconnect();
    }

    bool Networking::Initialize() {
        if (enet_initialize() != 0) {
            SKSE::log::error("Failed to initialize ENet.");
            return false;
        }

        m_client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (m_client == nullptr) {
            SKSE::log::error("Failed to create ENet client host.");
            return false;
        }

        SKSE::log::info("ENet initialized successfully.");
        return true;
    }

    void Networking::Connect(const std::string& host, uint16_t port) {
        if (!m_client) return;

        ENetAddress address;
        enet_address_set_host(&address, host.c_str());
        address.port = port;

        m_peer = enet_host_connect(m_client, &address, 2, 0);
        if (m_peer == nullptr) {
            SKSE::log::error("No available peers for initiating an ENet connection.");
            return;
        }

        m_running = true;
        m_networkThread = std::thread(&Networking::NetworkLoop, this);
        SKSE::log::info("Connecting to server {}:{}...", host, port);
    }

    void Networking::Disconnect() {
        m_running = false;
        if (m_networkThread.joinable()) {
            m_networkThread.join();
        }
        
        if (m_peer) {
            enet_peer_disconnect(m_peer, 0);
            
            ENetEvent event;
            while (enet_host_service(m_client, &event, 3000) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_RECEIVE:
                        enet_packet_destroy(event.packet);
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        SKSE::log::info("Disconnection succeeded.");
                        return;
                }
            }
            // Force reset if graceful disconnect fails
            enet_peer_reset(m_peer);
            m_peer = nullptr;
        }

        if (m_client) {
            enet_host_destroy(m_client);
            m_client = nullptr;
        }
        
        enet_deinitialize();
    }

#include "ActorManager.h"

    void Networking::NetworkLoop() {
        ENetEvent event;
        while (m_running) {
            while (enet_host_service(m_client, &event, 10) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        SKSE::log::info("Connected to server!");
                        break;
                    case ENET_EVENT_TYPE_RECEIVE: {
                        // Very simple parser for our specific JSON format: {"id":"ip:port","x":1,"y":2,"z":3,"rot":4}
                        std::string payload(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                        
                        char idBuffer[64] = {0};
                        float x = 0, y = 0, z = 0, rot = 0;
                        
                        if (sscanf_s(payload.c_str(), "{\"id\":\"%[^\"]\",\"x\":%f,\"y\":%f,\"z\":%f,\"rot\":%f}", 
                            idBuffer, (unsigned)_countof(idBuffer), &x, &y, &z, &rot) == 5) {
                            ActorManager::Get().UpdateRemotePlayer(idBuffer, x, y, z, rot);
                        }

                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT:
                        SKSE::log::info("Disconnected from server.");
                        m_peer = nullptr;
                        break;
                }
            }
        }
    }

    void Networking::SendPositionUpdate(float x, float y, float z, float rot) {
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;

        // Extremely simple JSON-like string for now to match our Node.js server.
        // For production, binary struct serialization is much faster.
        std::string payload = std::format("{{\"x\":{}, \"y\":{}, \"z\":{}, \"rot\":{}}}", x, y, z, rot);
        
        ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(m_peer, 0, packet);
    }
}
