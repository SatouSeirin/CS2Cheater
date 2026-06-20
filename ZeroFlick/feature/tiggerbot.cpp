#include "Windows.h"
#include "./tiggerbot.h"
#include <cstdint>
#include "../cs2 dumper/offsets.hpp"
#include "../cs2 dumper/client_dll.hpp"
#include "esp.h"
#include "../cs2 dumper/buttons.hpp"
#include "../gui/gui.h"

DWORD tiggerbot(void*) {
    const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));

    while (true) {
        if (!client) {
            Sleep(1000);
            continue;
        }

        auto local_ctrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
        if (!local_ctrl) {
            Sleep(1);
            continue;
        }

        auto local_hpawn = *reinterpret_cast<uint32_t*>(local_ctrl + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
        if (local_hpawn == 0xFFFFFFFF) {
            Sleep(1);
            continue;
        }

        auto localpawn = GetBaseEntityFromHandle(local_hpawn, client);
        if (!localpawn) {
            Sleep(1);
            continue;
        }

        auto localteam = *reinterpret_cast<uint8_t*>(localpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        auto localhealth = *reinterpret_cast<int*>(localpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        if (localhealth <= 0) {
            Sleep(1);
            continue;
        }

        auto crosshair_entity_handle = *reinterpret_cast<uint32_t*>(localpawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex);

        if (crosshair_entity_handle == 0 || crosshair_entity_handle == 0xFFFFFFFF) {
            Sleep(1);
            continue;
        }

        auto playerpawn = GetBaseEntityFromHandle(crosshair_entity_handle, client);
        if (!playerpawn) {
            Sleep(1);
            continue;
        }

        auto player_team = *reinterpret_cast<uint8_t*>(playerpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
        if (player_team == localteam) {
            Sleep(1);
            continue;
        }

        auto player_health = *reinterpret_cast<int*>(playerpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        auto player_iMaxhealth = *reinterpret_cast<int*>(playerpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iMaxHealth);

        if (localhealth <= 0 || player_health <= 0 || player_iMaxhealth <= 0) {
            Sleep(1);
            continue;
        }

        if ((GetAsyncKeyState(zeroflick::aim::triggerKey) & 0x8000 && zeroflick::aim::autoaim)) {
            // 模拟鼠标左键按下
            INPUT input_down = { 0 };
            input_down.type = INPUT_MOUSE;
            input_down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input_down, sizeof(INPUT));

            Sleep(10); // 短暂延迟模拟射击

            // 模拟鼠标左键释放
            INPUT input_up = { 0 };
            input_up.type = INPUT_MOUSE;
            input_up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input_up, sizeof(INPUT));

            Sleep(25); // 射击间隔
        }

        Sleep(1);
    }
    return 0;
}