#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <nlohmann/json.hpp>
#include <set>
#include "Networking.h"
#include "ActorManager.h"

using json = nlohmann::json;

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

        SKSE::log::info("Accessing UI singleton...");
        /* 
        // Temporarily disabled due to junk pointer crash (0x6e6576456563616c)
        auto ui = RE::UI::GetSingleton();
        if (ui) {
            SKSE::log::info("UI singleton found at {:p}. Registering event sink...", (void*)ui);
            auto* source = ui->GetEventSource<RE::MenuOpenCloseEvent>();
            if (source) {
                source->AddEventSink(this);
                SKSE::log::info("Registered UI Menu Event Sink");
            } else {
                SKSE::log::error("Failed to get MenuOpenCloseEvent source");
            }
        } else {
            SKSE::log::warn("UI singleton not found.");
        }
        */
        SKSE::log::info("UI registration skipped for diagnostics.");

        SKSE::log::info("About to start network thread...");
        m_running = true;
        try {
            m_networkThread = std::thread(&Networking::NetworkLoop, this);
            SKSE::log::info("Network thread started.");
        } catch (const std::exception& e) {
            SKSE::log::error("Failed to start network thread: {}", e.what());
            m_running = false;
            return false;
        }

        return true;
    }


    RE::BSEventNotifyControl Networking::ProcessEvent(const RE::MenuOpenCloseEvent*, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        return RE::BSEventNotifyControl::kContinue;
    }

    void Networking::Connect(const std::string& host, uint16_t port) {
        if (!m_client) return;

        m_lastHost = host;
        m_lastPort = port;

        ENetAddress address;
        enet_address_set_host(&address, host.c_str());
        address.port = port;

        m_peer = enet_host_connect(m_client, &address, 2, 0);
        if (m_peer == nullptr) {
            SKSE::log::error("No available peers for initiating an ENet connection.");
            return;
        }

        if (!m_running) {
            m_running = true;
            m_networkThread = std::thread(&Networking::NetworkLoop, this);
        }
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

    void Networking::NetworkLoop() {
        ENetEvent event;
        while (m_running) {
            if (enet_host_service(m_client, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        m_connected = true;
                        SKSE::log::info("Fellowship: Connected to server!");
                        AddNotification("Fellowship: Connected to server!");
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        try {
                            std::string payload(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                            auto j = json::parse(payload);
                            
                            std::string id = j.value("id", "unknown");
                            std::string type = j.value("type", "");

                            if (type == "pos") {
                                float x = j.value("x", 0.0f);
                                float y = j.value("y", 0.0f);
                                float z = j.value("z", 0.0f);
                                float rot = j.value("rot", 0.0f);
                                uint32_t cellID = j.value("cell", 0u);
                                uint32_t worldID = j.value("world", 0u);
                                
                                if (m_knownPlayers.find(id) == m_knownPlayers.end()) {
                                    SKSE::log::info("Player discovered: {}", id);
                                    AddNotification("Player Joined: " + id);
                                    m_knownPlayers.insert(id);
                                }
                                
                                ActorManager::Get().UpdateRemotePlayer(id, x, y, z, rot, cellID, worldID);
                            } else if (type == "anim") {
                                std::string animEvent = j.value("event", "");
                                SKSE::log::info("Anim from {}: {}", id, animEvent);
                                ActorManager::Get().PlayRemoteAnimation(id, animEvent);
                            }
                        } catch (const std::exception& e) {
                            SKSE::log::error("Failed to parse network packet: {}", e.what());
                        }

                        enet_packet_destroy(event.packet);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        SKSE::log::info("Disconnected from server.");
                        m_peer = nullptr;
                        m_connected = false;
                        break;
                }
            } else {
                if (m_peer == nullptr && !m_lastHost.empty()) {
                    static auto lastReconnectAttempt = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastReconnectAttempt).count() >= 5) {
                        lastReconnectAttempt = now;
                        SKSE::log::info("Attempting to reconnect to {}:{}...", m_lastHost, m_lastPort);
                        
                        ENetAddress address;
                        enet_address_set_host(&address, m_lastHost.c_str());
                        address.port = m_lastPort;
                        m_peer = enet_host_connect(m_client, &address, 2, 0);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void Networking::SendPositionUpdate(float x, float y, float z, float rot) {
        if (!m_peer) return;
        if (m_peer->state != ENET_PEER_STATE_CONNECTED) {
            static uint32_t peerStateLog = 0;
            if (++peerStateLog % 100 == 0) {
                SKSE::log::info("Peer not connected (state: {}), cannot send position.", (int)m_peer->state);
            }
            return;
        }

        static uint32_t sendCount = 0;
        if (++sendCount % 100 == 0) {
            SKSE::log::info("Sending position: [{:.1f}, {:.1f}, {:.1f}] rot: {:.1f}", x, y, z, rot);
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        uint32_t cellID = 0;
        uint32_t worldID = 0;

        if (player) {
            auto cell = player->GetParentCell();
            if (cell) cellID = cell->GetFormID();
            
            auto world = player->GetWorldspace();
            if (world) worldID = world->GetFormID();
        }

        json j;
        j["type"] = "pos";
        j["x"] = std::round(x * 100.0f) / 100.0f;
        j["y"] = std::round(y * 100.0f) / 100.0f;
        j["z"] = std::round(z * 100.0f) / 100.0f;
        j["rot"] = std::round(rot * 100.0f) / 100.0f;
        j["cell"] = cellID;
        j["world"] = worldID;

        std::string payload = j.dump();
        ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(m_peer, 0, packet);
    }

    void Networking::SendAnimationUpdate(const std::string& animEvent) {
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;

        json j;
        j["type"] = "anim";
        j["event"] = animEvent;

        std::string payload = j.dump();
        ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(m_peer, 0, packet);
    }

    void Networking::AddNotification(const std::string& msg) {
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        m_notifications.push_back(msg);
    }

    void Networking::ProcessNotifications() {
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        if (m_notifications.empty()) return;

        auto console = RE::ConsoleLog::GetSingleton();
        
        // Safety check for junk pointer (e.g. 0x6e6576456563616c)
        bool consoleValid = false;
        if (console) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(console);
            // Valid pointers in Skyrim are usually in the module range
            if (addr > 0x1000 && addr < 0x00007FFFFFFFFFFF) {
                consoleValid = true;
            }
        }

        for (const auto& msg : m_notifications) {
            RE::DebugNotification(msg.c_str());
            if (consoleValid) {
                console->Print("[Fellowship] %s", msg.c_str());
            }
        }
        m_notifications.clear();
    }
}
