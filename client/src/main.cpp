#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include "Networking.h"
#include "Hooks.h"
#include <spdlog/sinks/basic_file_sink.h>

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
    switch (a_msg->type) {
        case SKSE::MessagingInterface::kPostLoad:
            SKSE::log::info("kPostLoad received.");
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            SKSE::log::info("kDataLoaded received. Initializing networking...");
            if (Multiplayer::Networking::Get().Initialize()) {
                Multiplayer::Networking::Get().Connect("127.0.0.1", 3000);
            }
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("[Fellowship] Plugin active!");
            }
            Multiplayer::Hooks::Install();
            break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "Fellowship.log";

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log", std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("%g(%#): [%l] %v");

    SKSE::log::info("Fellowship Plugin Loaded.");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        return false;
    }

    return true;
}
