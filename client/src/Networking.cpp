#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "Networking.h"
#include "ActorManager.h"

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

        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(this);
            SKSE::log::info("Registered UI Menu Event Sink");
        }

        SKSE::log::info("About to start network thread...");
        m_running = true;
        m_networkThread = std::thread(&Networking::NetworkLoop, this);
        SKSE::log::info("Network thread started.");

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
            // Process networking
            if (enet_host_service(m_client, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        m_connected = true;
                        SKSE::log::info("Connected to server!");
                        AddNotification("Connected to Fellowship Server!");
                        

                        break;
                    case ENET_EVENT_TYPE_RECEIVE: {
                        std::string payload(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                        
                        auto get_value = [&](const std::string& key) -> std::string {
                            size_t pos = payload.find("\"" + key + "\":");
                            if (pos == std::string::npos) return "";
                            pos += key.length() + 3;
                            if (payload[pos] == '\"') {
                                pos++;
                                size_t end = payload.find("\"", pos);
                                return payload.substr(pos, end - pos);
                            } else {
                                size_t end = payload.find_first_of(",}", pos);
                                return payload.substr(pos, end - pos);
                            }
                        };

                        std::string id = get_value("id");
                        std::string type = get_value("type");

                        if (type == "pos") {
                            float x = std::strtof(get_value("x").c_str(), nullptr);
                            float y = std::strtof(get_value("y").c_str(), nullptr);
                            float z = std::strtof(get_value("z").c_str(), nullptr);
                            float rot = std::strtof(get_value("rot").c_str(), nullptr);
                            uint32_t cellID = (uint32_t)std::stoul(get_value("cell").empty() ? "0" : get_value("cell"));
                            uint32_t worldID = (uint32_t)std::stoul(get_value("world").empty() ? "0" : get_value("world"));
                            
                            static std::string lastId = "";
                            if (id != lastId) {
                                SKSE::log::info("Receiving data from new player: {}", id);
                                AddNotification("Player Joined: " + id);
                                lastId = id;
                            }

                            ActorManager::Get().UpdateRemotePlayer(id, x, y, z, rot, cellID, worldID);
                        } else if (type == "anim") {
                            std::string animEvent = get_value("event");
                            SKSE::log::info("Anim from {}: {}", id, animEvent);
                            ActorManager::Get().PlayRemoteAnimation(id, animEvent);
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
                // If we are not connected and not currently connecting, try to reconnect
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
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;

        auto player = RE::PlayerCharacter::GetSingleton();
        uint32_t cellID = 0;
        uint32_t worldID = 0;

        if (player) {
            auto cell = player->GetParentCell();
            if (cell) cellID = cell->GetFormID();
            
            auto world = player->GetWorldspace();
            if (world) worldID = world->GetFormID();
        }

        std::string payload = std::format("{{\"type\":\"pos\",\"x\":{:.2f},\"y\":{:.2f},\"z\":{:.2f},\"rot\":{:.2f},\"cell\":{},\"world\":{}}}", 
            x, y, z, rot, cellID, worldID);
        
        ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(m_peer, 0, packet);
    }

    void Networking::SendAnimationUpdate(const std::string& animEvent) {
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;

        std::string payload = std::format("{{\"type\":\"anim\",\"event\":\"{}\"}}", animEvent);
        
        // Animations should be reliable so they don't get dropped
        ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(m_peer, 0, packet);
    }

    void Networking::AddNotification(const std::string& msg) {
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        m_notifications.push_back(msg);
    }

    void Networking::ProcessNotifications() {


        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        for (const auto& msg : m_notifications) {
            RE::DebugNotification(msg.c_str());
            
            auto console = RE::ConsoleLog::GetSingleton();
            if (console) {
                console->Print("[Fellowship] %s", msg.c_str());
            }
        }
        m_notifications.clear();
    }
}
