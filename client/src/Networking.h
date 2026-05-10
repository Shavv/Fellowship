#pragma once
#include <enet/enet.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>

namespace Multiplayer {
    class Networking : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static Networking& Get();

        bool Initialize();
        void Connect(const std::string& host, uint16_t port);
        void Disconnect();

        // BSTEventSink override
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

        void ProcessNotifications();
        void AddNotification(const std::string& msg);

        void SendPositionUpdate(float x, float y, float z, float rot);
        void SendAnimationUpdate(const std::string& animEvent);

    private:
        Networking() = default;
        ~Networking();

        void NetworkLoop();

        ENetHost*  m_client{nullptr};
        ENetPeer*  m_peer{nullptr};
        std::thread m_networkThread;
        std::atomic<bool> m_running{false};
        std::string m_lastHost;
        uint16_t m_lastPort{0};
        bool m_connected{false};
        std::vector<std::string> m_notifications;
        std::set<std::string> m_knownPlayers;
        std::recursive_mutex m_notificationMutex;
    };
}
