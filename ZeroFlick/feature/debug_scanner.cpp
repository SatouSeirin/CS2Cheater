#include "debug_scanner.h"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include "../cs2 dumper/offsets.hpp"
#include "../cs2 dumper/client_dll.hpp"

namespace DebugScanner {

// 安全读取内存
template<typename T>
bool SafeRead(uintptr_t addr, T& out) {
    if (addr == 0 || IsBadReadPtr(reinterpret_cast<void*>(addr), sizeof(T)))
        return false;
    out = *reinterpret_cast<T*>(addr);
    return true;
}

void RunDiagnostics(uintptr_t clientBase) {
    printf("\n========== ZeroFlick Debug Scanner ==========\n");
    printf("client.dll base: 0x%p\n", (void*)clientBase);

    VerifyOffsets(clientBase);
    DumpLocalPlayer(clientBase);
    ScanEntityList(clientBase);

    printf("========== Diagnostics Complete ==========\n\n");
}

void VerifyOffsets(uintptr_t clientBase) {
    printf("\n--- [1] Verifying Global Offsets ---\n");

    // dwLocalPlayerController
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerController;
        uintptr_t localCtrl = 0;
        if (SafeRead(addr, localCtrl)) {
            printf("[OK] dwLocalPlayerController: addr=0x%p, value=0x%p\n", (void*)addr, (void*)localCtrl);
        } else {
            printf("[FAIL] dwLocalPlayerController: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }

    // dwLocalPlayerPawn
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn;
        uintptr_t localPawn = 0;
        if (SafeRead(addr, localPawn)) {
            printf("[OK] dwLocalPlayerPawn: addr=0x%p, value=0x%p\n", (void*)addr, (void*)localPawn);
        } else {
            printf("[FAIL] dwLocalPlayerPawn: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }

    // dwEntityList
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwEntityList;
        uintptr_t entList = 0;
        if (SafeRead(addr, entList)) {
            printf("[OK] dwEntityList: addr=0x%p, value=0x%p\n", (void*)addr, (void*)entList);
        } else {
            printf("[FAIL] dwEntityList: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }

    // dwViewMatrix
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwViewMatrix;
        uintptr_t matrixPtr = 0;
        if (SafeRead(addr, matrixPtr)) {
            printf("[OK] dwViewMatrix: addr=0x%p, value=0x%p\n", (void*)addr, (void*)matrixPtr);
            // 检查是否是指针还是直接矩阵
            if (matrixPtr > clientBase && matrixPtr < clientBase + 0x3000000) {
                printf("  -> dwViewMatrix is a POINTER (need dereference)\n");
            } else {
                printf("  -> dwViewMatrix might be direct matrix data\n");
            }
        } else {
            printf("[FAIL] dwViewMatrix: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }

    // dwViewAngles
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwViewAngles;
        uintptr_t viewAnglesPtr = 0;
        if (SafeRead(addr, viewAnglesPtr)) {
            printf("[OK] dwViewAngles: addr=0x%p, value=0x%p\n", (void*)addr, (void*)viewAnglesPtr);
            // 尝试读取角度值
            float x, y, z;
            if (SafeRead(viewAnglesPtr, x) && SafeRead(viewAnglesPtr + 4, y) && SafeRead(viewAnglesPtr + 8, z)) {
                printf("  -> Angles: (%.2f, %.2f, %.2f)\n", x, y, z);
            } else {
                printf("  -> Cannot read angles at 0x%p (maybe need double-dereference)\n", (void*)viewAnglesPtr);
            }
        } else {
            printf("[FAIL] dwViewAngles: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }

    // dwCSGOInput
    {
        uintptr_t addr = clientBase + cs2_dumper::offsets::client_dll::dwCSGOInput;
        uintptr_t csgoInput = 0;
        if (SafeRead(addr, csgoInput)) {
            printf("[OK] dwCSGOInput: addr=0x%p, value=0x%p\n", (void*)addr, (void*)csgoInput);
            if (csgoInput) {
                // 尝试读取虚函数表
                uintptr_t vtable = 0;
                if (SafeRead(csgoInput, vtable)) {
                    printf("  -> vtable: 0x%p\n", (void*)vtable);
                    // 尝试读取 CreateMove (vtable[21])
                    uintptr_t vtableEntry = 0;
                    if (SafeRead(vtable + 21 * 8, vtableEntry)) {
                        printf("  -> vtable[21] (CreateMove): 0x%p\n", (void*)vtableEntry);
                    }
                }
            }
        } else {
            printf("[FAIL] dwCSGOInput: addr=0x%p is NOT readable!\n", (void*)addr);
        }
    }
}

void DumpLocalPlayer(uintptr_t clientBase) {
    printf("\n--- [2] Dumping LocalPlayer ---\n");

    uintptr_t localCtrl = 0;
    uintptr_t ctrlAddr = clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerController;
    if (!SafeRead(ctrlAddr, localCtrl) || !localCtrl) {
        printf("[FAIL] Cannot read LocalPlayerController!\n");
        return;
    }
    printf("LocalPlayerController: 0x%p\n", (void*)localCtrl);

    // 读取 m_hPawn (handle)
    uint32_t hPawn = 0;
    if (SafeRead(localCtrl + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn, hPawn)) {
        printf("  m_hPlayerPawn handle (0x90C): 0x%X (index=%d)\n", hPawn, hPawn & 0x7FFF);
    } else {
        printf("[FAIL] Cannot read m_hPlayerPawn at 0x90C!\n");
        return;
    }

    if (hPawn == 0xFFFFFFFF) {
        printf("  -> Pawn handle is INVALID (0xFFFFFFFF), player not spawned!\n");
        return;
    }

    // 通过句柄获取Pawn实体
    uintptr_t entListBase = 0;
    if (!SafeRead(clientBase + cs2_dumper::offsets::client_dll::dwEntityList, entListBase) || !entListBase) {
        printf("[FAIL] Cannot read dwEntityList!\n");
        return;
    }

    int nIndex = hPawn & 0x7FFF;
    uintptr_t chunkAddr = entListBase + 8 * (nIndex >> 9) + 16;
    uintptr_t entityListChunk = 0;
    if (!SafeRead(chunkAddr, entityListChunk) || !entityListChunk) {
        printf("[FAIL] Cannot read entity list chunk at 0x%p!\n", (void*)chunkAddr);
        return;
    }

    uintptr_t pawnAddr = entityListChunk + 0x78 * (nIndex & 0x1FF);
    uintptr_t localPawn = 0;
    if (!SafeRead(pawnAddr, localPawn) || !localPawn) {
        printf("[FAIL] Cannot read pawn entity at 0x%p!\n", (void*)pawnAddr);
        return;
    }
    printf("  LocalPawn entity: 0x%p\n", (void*)localPawn);

    // 读取关键 Schema 字段
    // m_iTeamNum (uint8 at 0x3EB)
    {
        uint8_t team = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum, team)) {
            printf("  m_iTeamNum (uint8): %d\n", team);
        } else {
            printf("[FAIL] Cannot read m_iTeamNum at 0x3EB!\n");
        }
    }

    // m_iHealth (int32 at 0x34C)
    {
        int health = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth, health)) {
            printf("  m_iHealth: %d\n", health);
        } else {
            printf("[FAIL] Cannot read m_iHealth!\n");
        }
    }

    // m_iMaxHealth (int32 at 0x348)
    {
        int maxHealth = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iMaxHealth, maxHealth)) {
            printf("  m_iMaxHealth: %d\n", maxHealth);
        } else {
            printf("[FAIL] Cannot read m_iMaxHealth!\n");
        }
    }

    // m_iIDEntIndex (CEntityIndex at 0x33FC)
    {
        uint32_t idEntIndex = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex, idEntIndex)) {
            printf("  m_iIDEntIndex: 0x%X (index=%d)\n", idEntIndex, idEntIndex & 0x7FFF);
        } else {
            printf("[FAIL] Cannot read m_iIDEntIndex!\n");
        }
    }

    // m_iShotsFired (int32 at 0x1C64)
    {
        int shotsFired = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired, shotsFired)) {
            printf("  m_iShotsFired: %d\n", shotsFired);
        } else {
            printf("[FAIL] Cannot read m_iShotsFired!\n");
        }
    }

    // m_vOldOrigin (Vector at 0x1390)
    {
        float x, y, z;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin, x) &&
            SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 4, y) &&
            SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 8, z)) {
            printf("  m_vOldOrigin: (%.2f, %.2f, %.2f)\n", x, y, z);
        } else {
            printf("[FAIL] Cannot read m_vOldOrigin!\n");
        }
    }

    // m_fFlags (uint32 at 0x3F8)
    {
        uint32_t flags = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags, flags)) {
            printf("  m_fFlags: 0x%X (FL_ONGROUND=%d)\n", flags, (flags & 1) ? 1 : 0);
        } else {
            printf("[FAIL] Cannot read m_fFlags!\n");
        }
    }

    // m_pAimPunchServices
    {
        uintptr_t aimPunchServices = 0;
        if (SafeRead(localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices, aimPunchServices)) {
            printf("  m_pAimPunchServices: 0x%p\n", (void*)aimPunchServices);
            if (aimPunchServices) {
                float punchX, punchY, punchZ;
                if (SafeRead(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_unpredictableBaseAngle, punchX) &&
                    SafeRead(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_unpredictableBaseAngle + 4, punchY) &&
                    SafeRead(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_unpredictableBaseAngle + 8, punchZ)) {
                    printf("  m_unpredictableBaseAngle: (%.4f, %.4f, %.4f)\n", punchX, punchY, punchZ);
                }
            }
        } else {
            printf("[FAIL] Cannot read m_pAimPunchServices!\n");
        }
    }
}

void ScanEntityList(uintptr_t clientBase) {
    printf("\n--- [3] Scanning Entity List (first 32 entries) ---\n");

    int validEntities = 0;
    int playerCount = 0;

    for (int i = 0; i < 32; i++) {
        // 使用和 GetBaseEntity 相同的逻辑
        uintptr_t entListBase = 0;
        if (!SafeRead(clientBase + cs2_dumper::offsets::client_dll::dwEntityList, entListBase) || !entListBase)
            break;

        uintptr_t chunkAddr = entListBase + 8 * (i >> 9) + 16;
        uintptr_t entityListChunk = 0;
        if (!SafeRead(chunkAddr, entityListChunk) || !entityListChunk)
            continue;

        uintptr_t entityAddr = entityListChunk + 0x78 * (i & 0x1FF);
        uintptr_t entity = 0;
        if (!SafeRead(entityAddr, entity) || !entity)
            continue;

        validEntities++;

        // 尝试读取 m_hPawn 判断是否是玩家控制器
        uint32_t hPlayerPawn = 0;
        if (SafeRead(entity + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn, hPlayerPawn)) {
            if (hPlayerPawn != 0 && hPlayerPawn != 0xFFFFFFFF) {
                playerCount++;

                // 获取 Pawn
                int pawnIdx = hPlayerPawn & 0x7FFF;
                uintptr_t pawnChunkAddr = entListBase + 8 * (pawnIdx >> 9) + 16;
                uintptr_t pawnChunk = 0;
                if (SafeRead(pawnChunkAddr, pawnChunk) && pawnChunk) {
                    uintptr_t pawnEntityAddr = pawnChunk + 0x78 * (pawnIdx & 0x1FF);
                    uintptr_t pawn = 0;
                    if (SafeRead(pawnEntityAddr, pawn) && pawn) {
                        // 读取队伍和血量
                        uint8_t team = 0;
                        int health = 0;
                        SafeRead(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum, team);
                        SafeRead(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth, health);

                        printf("  [%d] Ctrl=0x%p, Pawn=0x%p, Team=%d, HP=%d\n",
                            i, (void*)entity, (void*)pawn, team, health);
                    }
                }
            }
        }
    }

    printf("  Total valid entities: %d, Players: %d\n", validEntities, playerCount);
}

} // namespace DebugScanner
