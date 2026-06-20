#include <Windows.h>
#include <stdio.h>
#include "../utils/Vector.h"
#include "../cs2 dumper/client_dll.hpp"
#include "../cs2 dumper/offsets.hpp"
#include "../gui/gui.h"

// dwViewAngles 是指向 ViewAngle 数据的全局指针，需要二次解引用
Vector3 get_view_anlge(uintptr_t client) {
    // 第一步：读取 dwViewAngles 偏移处的指针
    auto pViewAnglesPtr = reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwViewAngles);
    if (!pViewAnglesPtr || IsBadReadPtr(pViewAnglesPtr, sizeof(uintptr_t)))
        return Vector3{};
    // 第二步：该指针指向实际的 Vector3 角度数据
    auto viewAngles = reinterpret_cast<Vector3*>(*pViewAnglesPtr);
    if (!viewAngles || IsBadReadPtr(viewAngles, sizeof(Vector3)))
        return Vector3{};
    return *viewAngles;
}

void set_view_anlge(Vector3 Angle, uintptr_t client) {
    auto pViewAnglesPtr = reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwViewAngles);
    if (!pViewAnglesPtr || IsBadReadPtr(pViewAnglesPtr, sizeof(uintptr_t)))
        return;
    auto viewAngles = reinterpret_cast<Vector3*>(*pViewAnglesPtr);
    if (!viewAngles || IsBadReadPtr(viewAngles, sizeof(Vector3)))
        return;
    *viewAngles = Angle;
}

void NormalizePitch(float& pPitch)// 89 to -89
{
    pPitch = (pPitch < -89.0f) ? -89.0f : pPitch;

    pPitch = (pPitch > 89.f) ? 89.0f : pPitch;
}

void NormalizeYaw(float& pYaw)// 180 to -180
{
    while (pYaw > 180.f) pYaw -= 360.f;

    while (pYaw < -180.f) pYaw += 360.f;
}


DWORD autopunch(void* arg) {
    AllocConsole();
    FILE* file;
    freopen_s(&file, "CONOUT$", "w", stdout);

    const static auto client = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));

    Vector3 old_aimpunch{};
    bool back_first = false;


    while (1) {

        auto localplayer_pawn = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
        if (localplayer_pawn == 0 || !localplayer_pawn)
        {
            Sleep(1);
            continue;
        }

        // 安全检查：验证 localplayer_pawn 是否可读
        if (IsBadReadPtr(reinterpret_cast<void*>(localplayer_pawn), sizeof(uintptr_t)))
        {
            Sleep(1);
            continue;
        }

        auto health = *reinterpret_cast<int*>(localplayer_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);

        if (health <= 0)
        {
            Sleep(1);
            continue;
        }

        auto i_viewAngles = get_view_anlge(client);

        // 新版本：通过 m_pAimPunchServices 获取后坐力角度
        uintptr_t aimPunchServices = 0;
        if (!IsBadReadPtr(reinterpret_cast<void*>(localplayer_pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices), sizeof(uintptr_t))) {
            aimPunchServices = *reinterpret_cast<uintptr_t*>(localplayer_pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices);
        }
        Vector3 local_aimpunch{};
        if (aimPunchServices && !IsBadReadPtr(reinterpret_cast<void*>(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_unpredictableBaseAngle), sizeof(Vector3))) {
            auto aimPunchAnglePtr = reinterpret_cast<Vector3*>(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_unpredictableBaseAngle);
            local_aimpunch = *aimPunchAnglePtr;
        }

        auto iShotsFired = *reinterpret_cast<int*>(localplayer_pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired);

        if (iShotsFired && GetAsyncKeyState(zeroflick::aim::aimKey) & 0x8000 && zeroflick::aim::autopunch) {

            if (!back_first)
            {
                back_first = true;
            }
            Vector3 new_viewAngles{};

            new_viewAngles.x = i_viewAngles.x + old_aimpunch.x - (local_aimpunch.x * 2.f);
            new_viewAngles.y = i_viewAngles.y + old_aimpunch.y - (local_aimpunch.y * 2.f);

            NormalizePitch(new_viewAngles.x);
            NormalizeYaw(new_viewAngles.y);

            set_view_anlge(new_viewAngles, client);

            old_aimpunch.x = local_aimpunch.x * 2.f;
            old_aimpunch.y = local_aimpunch.y * 2.f;
        }
        else {
            if (back_first)
            {
                Vector3 back_firstAngle{};

                back_firstAngle.x = i_viewAngles.x + old_aimpunch.x;
                back_firstAngle.y = i_viewAngles.y + old_aimpunch.y;

                set_view_anlge(back_firstAngle, client);

                back_first = false;
            }
            old_aimpunch = { 0.f, 0.f, 0.f };
        }

        Sleep(1);
    }


    return 1;
}