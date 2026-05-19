#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "Networking.h"
#include "Hooks.h"
#include "Version.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <format>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    switch (a_msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            SKSE::log::info("kDataLoaded: Data loaded. Initializing hooks and networking...");
            Multiplayer::Hooks::Install();
            if (Multiplayer::Networking::Get().Initialize()) {
                SKSE::log::info("Fellowship: Networking initialized. Connecting...");
                Multiplayer::Networking::Get().Connect("127.0.0.1", 3000);
            }
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            SKSE::log::info("kPostLoadGame/kNewGame: Entering game world. Sending game loaded signal...");
            Multiplayer::Hooks::NotifyGameLoaded();
            break;
    }
}

SKSEPluginInfo(
    .Version = REL::Version{ 1, 0, 0, 0 },
    .Name = "Fellowship",
    .Author = "Shavv",
    .SupportEmail = "",
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary },
    .MinimumSKSEVersion = REL::Version{ 2, 2, 6, 0 }
)

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    auto path = SKSE::log::log_directory();
    if (!path) return false;
    
    *path /= "Fellowship.log";

    try {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log  = std::make_shared<spdlog::logger>("global log", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] %g(%#): [%l] %v");
    } catch (...) {
        return false;
    }

    SKSE::log::info("Fellowship Plugin Loaded. Version: 1.0.0");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) {
        SKSE::log::error("Failed to register messaging listener.");
        return false;
    }

    return true;
}
