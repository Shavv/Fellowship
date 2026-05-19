#pragma once
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <set>
#include <mutex>

namespace Multiplayer {
    class Networking : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static Networking& Get();
        bool Initialize();
        std::string NormalizeID(const std::string& id);
        std::string GetMyId();
        void Connect(const std::string& host, uint16_t port);
        void Disconnect();

        // BSTEventSink override
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

        void ProcessNotifications();
        void AddNotification(const std::string& msg);

        void SendPositionUpdate(float x, float y, float z, float rot);
        void SendSpawnLog(const std::string& remoteId);

    private:
        Networking() = default;
        ~Networking();

        void NetworkLoop();
        void HandlePacket(const std::string& id, const nlohmann::json& j);

        ENetHost*  m_client{nullptr};
        ENetPeer*  m_peer{nullptr};
        std::thread m_networkThread;
        std::atomic<bool> m_running{false};
        std::string m_lastHost;
        uint16_t m_lastPort{0};
        bool m_connected{false};
        std::string m_myId;
        std::vector<std::string> m_notifications;
        std::vector<std::string> m_consoleQueue;
        std::set<std::string> m_knownPlayers;
        std::recursive_mutex m_notificationMutex;
        std::mutex m_enetMutex;
        std::mutex m_myIdMutex;
    };
}
