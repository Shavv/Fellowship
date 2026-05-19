#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <nlohmann/json.hpp>
#include <set>
#include "Networking.h"
#include "ActorManager.h"
#include "Version.h"

using json = nlohmann::json;

namespace Multiplayer {
    Networking& Networking::Get() {
        static Networking instance;
        return instance;
    }

    std::string Networking::NormalizeID(const std::string& id) {
        if (id.starts_with("::ffff:")) {
            return id.substr(7);
        }
        return id;
    }

    std::string Networking::GetMyId() {
        std::lock_guard<std::mutex> lock(m_myIdMutex);
        return m_myId;
    }

    Networking::~Networking() {
        Disconnect();
    }

    bool Networking::Initialize() {
        SKSE::log::info("Networking: Initialize() called.");
        if (m_running) {
            SKSE::log::info("Networking: Already running.");
            return true;
        }

        if (enet_initialize() != 0) {
            SKSE::log::error("Networking: Failed to initialize ENet.");
            return false;
        }

        m_client = enet_host_create(nullptr, 1, 2, 0, 0);
        if (m_client == nullptr) {
            SKSE::log::error("Networking: Failed to create ENet client host.");
            return false;
        }

        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(this);
            SKSE::log::info("Networking: MenuOpenCloseEvent sink registered.");
        } else {
            SKSE::log::error("Networking: Failed to get UI singleton to register event sink.");
        }

        m_running = true;
        try {
            m_networkThread = std::thread(&Networking::NetworkLoop, this);
            SKSE::log::info("Networking: Network thread started.");
        } catch (const std::exception& e) {
            SKSE::log::error("Networking: Failed to start network thread: {}", e.what());
            m_running = false;
            return false;
        }

        return true;
    }

    void Networking::Connect(const std::string& host, uint16_t port) {
        if (!m_client) {
            SKSE::log::error("Networking: Cannot connect - ENet host not created.");
            return;
        }

        m_lastHost = host;
        m_lastPort = port;

        ENetAddress address;
        if (enet_address_set_host(&address, host.c_str()) < 0) {
            SKSE::log::error("Networking: Failed to resolve host: {}", host);
            return;
        }
        address.port = port;

        SKSE::log::info("Networking: Initiating connection to server {}:{}...", host, port);
        
        std::lock_guard<std::mutex> lock(m_enetMutex);
        if (m_peer) {
            SKSE::log::warn("Networking: Peer already exists, resetting...");
            enet_peer_reset(m_peer);
        }

        m_peer = enet_host_connect(m_client, &address, 2, 0);
        
        if (m_peer == nullptr) {
            SKSE::log::error("Networking: No available peers for connection (enet_host_connect failed).");
            return;
        }
        
        SKSE::log::info("Networking: Connection attempt started (Peer state: {})", static_cast<int>(m_peer->state));
    }

    void Networking::Disconnect() {
        m_running = false;
        if (m_networkThread.joinable()) {
            m_networkThread.join();
        }
        
        std::lock_guard<std::mutex> lock(m_enetMutex);
        if (m_peer) {
            enet_peer_disconnect(m_peer, 0);
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
            bool hasEvent = false;
            {
                std::lock_guard<std::mutex> lock(m_enetMutex);
                if (enet_host_service(m_client, &event, 0) > 0) {
                    hasEvent = true;
                }
            }

            if (hasEvent) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        m_connected = true;
                        SKSE::log::info("Networking: CONNECT event received! Connected to server.");
                        AddNotification("Connected to server successfully.");
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
                            m_consoleQueue.push_back("Connected to server successfully.");
                        }
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        try {
                            std::string payload(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                            auto j = json::parse(payload);
                            std::string id = j.value("id", "unknown");
                            
                            std::string myId = NormalizeID(GetMyId());
                            std::string incomingId = NormalizeID(id);
                            
                            if (!myId.empty() && incomingId == myId) {
                                static auto lastSelfLog = std::chrono::steady_clock::now();
                                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastSelfLog).count() >= 5) {
                                    SKSE::log::info("Networking: Self-packet caught (ID match: {}). Filtering...", incomingId);
                                    lastSelfLog = std::chrono::steady_clock::now();
                                }
                            } else {
                                HandlePacket(incomingId, j);
                            }
                        } catch (const std::exception& e) {
                            SKSE::log::error("Networking: Packet parse error: {}", e.what());
                        }
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        m_connected = false;
                        m_peer = nullptr;
                        SKSE::log::warn("Networking: DISCONNECT event received. Connection lost.");
                        AddNotification("Disconnected from server.");
                        break;
                }
            } else {
                // Auto-reconnect if needed
                if (m_peer == nullptr && !m_lastHost.empty()) {
                    static auto lastReconnect = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - lastReconnect).count() >= 5) {
                        lastReconnect = std::chrono::steady_clock::now();
                        SKSE::log::info("Networking: Auto-reconnect triggered...");
                        Connect(m_lastHost, m_lastPort);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    void Networking::HandlePacket(const std::string& id, const json& j) {
        std::string type = j.value("type", "");
        if (type == "welcome") {
            {
                std::lock_guard<std::mutex> lock(m_myIdMutex);
                m_myId = NormalizeID(j.value("id", ""));
            }
            SKSE::log::info("Networking: Received welcome. My normalized ID is: '{}'", m_myId);
            // Strictly ensure we don't have ourselves in the remote players list
            if (!m_myId.empty()) {
                ActorManager::Get().RemovePlayer(m_myId); 
            }
        } else if (type == "pos") {
            std::string myId = GetMyId();
            
            // CRITICAL: Do not process ANY position updates until we know our own ID.
            // This prevents us from spawning a 'dummy' of ourselves before we can filter it out.
            if (myId.empty()) {
                return;
            }

            if (id == myId) {
                return;
            }

            float x = j.value("x", 0.0f);
            float y = j.value("y", 0.0f);
            float z = j.value("z", 0.0f);
            float rot = j.value("rot", 0.0f);
            float vx = j.value("vx", 0.0f);
            float vy = j.value("vy", 0.0f);
            float vz = j.value("vz", 0.0f);
            uint32_t cellID = j.value("cell", (uint32_t)0);
            uint32_t worldID = j.value("world", (uint32_t)0);

            // PERSISTENT PROXIMITY FILTER: 
            // If a player is exactly on top of us, it is almost certainly a reflected packet 
            // from a 'Ghost' session (previous connection) that hasn't timed out yet.
            auto player = Fellowship::GetPlayer();
            if (player) {
                RE::NiPoint3 localPos = player->GetPosition();
                float distSq = std::pow(x - localPos.x, 2) + std::pow(y - localPos.y, 2) + std::pow(z - localPos.z, 2);
                
                // If they are within 5.0 units (very close), we check if they are a 'Self-Ghost'
                if (distSq < 25.0f) { 
                    // If we ARE already tracking them, we only allow the update if they were PREVIOUSLY far away.
                    // This prevents ghost sessions from 'latching on' to the player.
                    static std::set<std::string> suspectedGhosts;
                    
                    if (!ActorManager::Get().IsTracking(id)) {
                        // Brand new player appearing inside our model? Red flag.
                        return;
                    }
                }
            }

            ActorManager::Get().UpdateRemotePlayer(id, x, y, z, rot, vx, vy, vz, cellID, worldID);
        }
    }

    void Networking::SendPositionUpdate(float x, float y, float z, float rot) {
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;
        
        std::string myId = GetMyId();
        if (myId.empty()) return;

        // Safety: Ignore (0,0,0) which often indicates uninitialized state or loading
        if (x == 0.0f && y == 0.0f && z == 0.0f) return;

        auto player = Fellowship::GetPlayer();
        uint32_t cellID = 0;
        uint32_t worldID = 0;
        if (player) {
            if (auto cell = player->GetParentCell()) cellID = cell->GetFormID();
            if (auto world = player->GetWorldspace()) worldID = world->GetFormID();
        }

        json j;
        j["type"] = "pos";
        j["id"] = GetMyId();
        j["x"] = std::round(x * 100.0f) / 100.0f;
        j["y"] = std::round(y * 100.0f) / 100.0f;
        j["z"] = std::round(z * 100.0f) / 100.0f;
        j["rot"] = std::round(rot * 100.0f) / 100.0f;

        RE::NiPoint3 vel{0.0f, 0.0f, 0.0f};
        if (player) {
            player->GetLinearVelocity(vel);
        }
        j["vx"] = std::round(vel.x * 100.0f) / 100.0f;
        j["vy"] = std::round(vel.y * 100.0f) / 100.0f;
        j["vz"] = std::round(vel.z * 100.0f) / 100.0f;

        j["cell"] = cellID;
        j["world"] = worldID;

        std::string payload = j.dump();
        std::lock_guard<std::mutex> lock(m_enetMutex);
        if (m_peer) {
            ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_UNSEQUENCED);
            enet_peer_send(m_peer, 0, packet);
        }
    }

    void Networking::SendSpawnLog(const std::string& remoteId) {
        if (!m_peer || m_peer->state != ENET_PEER_STATE_CONNECTED) return;
        
        std::string myId = GetMyId();
        if (myId.empty()) return;

        json j;
        j["type"] = "spawn_log";
        j["id"] = myId;
        j["msg"] = "Spawned dummy actor for player " + remoteId;

        std::string payload = j.dump();
        std::lock_guard<std::mutex> lock(m_enetMutex);
        if (m_peer) {
            ENetPacket* packet = enet_packet_create(payload.c_str(), payload.length() + 1, ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(m_peer, 0, packet);
        }
    }

    void Networking::AddNotification(const std::string& msg) {
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        m_notifications.push_back(msg);
    }

    void Networking::ProcessNotifications() {
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        
        auto ui = Fellowship::GetUI();
        bool menusOpen = ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME));

        if (!menusOpen) {
            for (const auto& msg : m_notifications) {
                RE::DebugNotification(msg.c_str());
            }
            m_notifications.clear();
        }

        // Console messages are safe to print even when menus are open
        for (const auto& msg : m_consoleQueue) {
            Fellowship::PrintToConsole(msg);
        }
        m_consoleQueue.clear();
    }

    RE::BSEventNotifyControl Networking::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (a_event) {
            if (a_event->menuName == RE::MainMenu::MENU_NAME && a_event->opening) {
                static bool printed = false;
                if (!printed) {
                    Fellowship::PrintToConsole("Plugin Loaded Successfully.");
                    SKSE::log::info("Fellowship: Main Menu opened. Printed loaded message to console.");
                    printed = true;
                }
            }
        }

        // Process console queue whenever any menu event occurs to capture early connection statuses
        std::lock_guard<std::recursive_mutex> lock(m_notificationMutex);
        for (const auto& msg : m_consoleQueue) {
            Fellowship::PrintToConsole(msg);
        }
        m_consoleQueue.clear();

        return RE::BSEventNotifyControl::kContinue;
    }
}
