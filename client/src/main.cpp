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
            SKSE::log::info("kDataLoaded received. Initializing Fellowship components...");
            
            {
                auto player = RE::PlayerCharacter::GetSingleton();
                auto ui = RE::UI::GetSingleton();
                auto calendar = RE::Calendar::GetSingleton();
                auto console = RE::ConsoleLog::GetSingleton();

                SKSE::log::info("Initial Singleton Check:");
                SKSE::log::info("  Player:   {:p}", (void*)player);
                SKSE::log::info("  UI:       {:p}", (void*)ui);
                SKSE::log::info("  Calendar: {:p}", (void*)calendar);
                SKSE::log::info("  Console:  {:p}", (void*)console);

                // Check for known junk pointers
                if ((uintptr_t)player == 0x6e6576456563616c || (uintptr_t)ui == 0x6e6576456563616c) {
                    SKSE::log::error("CRITICAL: Detected junk pointers from Address Library. Singletons are invalid!");
                    SKSE::log::error("This usually means the Address Library version does not match the game version (1.6.1170).");
                    // We will continue for now just to see all logs, but we won't initialize networking yet.
                }
            }

            // Networking can be initialized as soon as data is loaded.
            if (Multiplayer::Networking::Get().Initialize()) {
                SKSE::log::info("Fellowship: Networking initialized. Connecting...");
                Multiplayer::Networking::Get().Connect("127.0.0.1", 3000);
            }
            
            SKSE::log::info("Installing hooks...");
            Multiplayer::Hooks::Install();
            SKSE::log::info("Hooks installed.");
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            SKSE::log::info("--- Game Loaded Message ---");
            SKSE::log::info("Player Singleton: {:p}", (void*)RE::PlayerCharacter::GetSingleton());
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

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log", std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("%g(%#): [%l] %v");

    SKSE::log::info("Fellowship Plugin Loaded (v1.0.0).");
    SKSE::log::info("Runtime Version: {}", skse->RuntimeVersion().string());

    if (REL::Module::IsAE()) {
        SKSE::log::info("Running on AE.");
        SKSE::log::info("Module Base: {:p}", (void*)REL::Module::get().base());
        try {
            auto offset = REL::ID(517014).offset();
            SKSE::log::info("Address Library: ID 517014 maps to offset 0x{:X}", offset);
        } catch (...) {
            SKSE::log::error("Address Library: Failed to lookup ID 517014");
        }
    }

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener(MessageHandler)) {
        return false;
    }

    return true;
}
