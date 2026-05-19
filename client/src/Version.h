#pragma once
#include <SKSE/SKSE.h>

namespace Fellowship {
    inline bool IsAE() {
        auto runtimeVersion = REL::Module::get().version();
        return (runtimeVersion.major() == 1 && runtimeVersion.minor() >= 6) || (runtimeVersion.major() > 1);
    }

    inline std::uint64_t GetPlayerID() {
        return IsAE() ? 403521 : 517014;
    }

    inline std::uint64_t GetConsoleID() {
        return IsAE() ? 401560 : 515064;
    }

    inline RE::PlayerCharacter* GetPlayer() {
        try {
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(player);
                if (addr > 0x1000 && addr < 0x00007FFFFFFFFFFF && addr != 0x6e6576456563616c) {
                    return player;
                }
            }
        } catch (...) {}
        return nullptr;
    }

    inline RE::ConsoleLog* GetConsole() {
        try {
            auto console = RE::ConsoleLog::GetSingleton();
            if (console) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(console);
                if (addr > 0x1000 && addr < 0x00007FFFFFFFFFFF && addr != 0x6e6576456563616c) {
                    return console;
                }
            }
        } catch (...) {}
        return nullptr;
    }

    inline RE::UI* GetUI() {
        try {
            auto ui = RE::UI::GetSingleton();
            if (ui) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(ui);
                if (addr > 0x1000 && addr < 0x00007FFFFFFFFFFF && addr != 0x6e6576456563616c) {
                    return ui;
                }
            }
        } catch (...) {}
        return nullptr;
    }

    inline bool PrintToConsole(const std::string& msg) {
        try {
            auto console = GetConsole();
            if (console) {
                console->Print("[Fellowship] %s", msg.c_str());
                return true;
            }
        } catch (...) {}
        return false;
    }
}
