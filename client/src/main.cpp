#include <SKSE/SKSE.h>

#include "Networking.h"
#include "Hooks.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            SKSE::log::info("Data loaded. Multiplayer Mod initializing.");
            if (Multiplayer::Networking::Get().Initialize()) {
                Multiplayer::Networking::Get().Connect("127.0.0.1", 3000);
            }
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    
    // Set up logging
    auto path = SKSE::log::log_directory();
    if (!path) {
        return false;
    }
    *path /= "SkyrimMultiplayerMod.log";
    SKSE::log::add_papyrus_sink(std::regex("SkyrimMultiplayerMod"));
    
    SKSE::log::info("Skyrim Multiplayer Mod loaded");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        return false;
    }

    Multiplayer::Hooks::Install();

    return true;
}
