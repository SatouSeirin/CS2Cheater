#include "Windows.h"
#include <cstdint>
#include "../cs2 dumper/offsets.hpp"
#include "../cs2 dumper/client_dll.hpp"
#include "../cs2 dumper/server_dll.hpp"
#include "../utils/Vector.h"
#include <optional>
#include <cmath>
#include "../imgui_d11/imgui.h"
#include "../gui/gui.h"
#include "esp.h"
#include <algorithm>
#include <random>
#include <string>
#include <unordered_map>


// ===== 实体列表布局自动发现 =====
struct EntityListConfig {
	uint32_t chunkArrayOffset = 0x10;   // chunk指针数组相对entSystem的偏移
	uint32_t entitySlotSize   = 0x70;   // 每个实体的槽位大小
	bool     discovered        = false;
};
EntityListConfig g_EntConfig;

void DiscoverEntityLayout(uintptr_t client) {
	if (g_EntConfig.discovered) return;

	auto entSystem = *reinterpret_cast<std::uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwEntityList);
	if (!entSystem) return;

	// 获取已知参考: localPawn 的地址和索引
	auto localPawn = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
	if (!localPawn) return;

	auto localCtrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
	if (!localCtrl) return;

	auto hPawn = *reinterpret_cast<uint32_t*>(localCtrl +
		cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
	if (hPawn == 0xFFFFFFFF) return;

	int knownIndex  = hPawn & 0x7FFF;
	int knownChunk  = knownIndex >> 9;
	int knownSlot   = knownIndex & 0x1FF;

	printf("[EntityLayout] Scanning: localPawn=0x%p index=%d chunk=%d slot=%d\n",
		(void*)localPawn, knownIndex, knownChunk, knownSlot);

	// 候选槽位大小
	const uint32_t slotSizes[] = { 0x70, 0x68, 0x78, 0x60, 0x80, 0x88, 0x90 };

	// 扫描 entSystem, 找哪个指针能指向 localPawn 所在 chunk
	// 扫描范围: entSystem + 0x00 ~ entSystem + 0x200, 步长 8 字节
	for (int off = 0x00; off <= 0x200; off += 8) {
		if (IsBadReadPtr(reinterpret_cast<void*>(entSystem + off), 8))
			continue;
		uintptr_t candidatePtr = *reinterpret_cast<uintptr_t*>(entSystem + off);
		if (!candidatePtr || candidatePtr < 0x10000ULL)
			continue;

		for (auto slotSz : slotSizes) {
			for (int maskMode = 0; maskMode < 2; maskMode++) {
				uintptr_t testChunk = candidatePtr;
				if (maskMode == 1)
					testChunk &= ~0x7ULL;

				uintptr_t testAddr = testChunk + static_cast<uintptr_t>(knownSlot) * slotSz;
				if (IsBadReadPtr(reinterpret_cast<void*>(testAddr), 8))
					continue;

				if (*reinterpret_cast<uintptr_t*>(testAddr) == localPawn) {
					// 找到了! 反推 chunkArrayOffset
					g_EntConfig.chunkArrayOffset = static_cast<uint32_t>(off - 8ULL * knownChunk);
					g_EntConfig.entitySlotSize   = slotSz;
					g_EntConfig.discovered       = true;

					printf("[EntityLayout] FOUND! chunkArrayOffset=0x%X slotSize=0x%X mask=%s (entSystem+0x%X -> chunk%d base)\n",
						g_EntConfig.chunkArrayOffset, g_EntConfig.entitySlotSize,
						maskMode ? "ON" : "OFF", off, knownChunk);
					return;
				}
			}
		}
	}

	// 回退到硬编码默认值
	printf("[EntityLayout] Auto-discovery failed, using defaults (offset=0x10, slotSize=0x70)\n");
	g_EntConfig.discovered = true;
}
// ===== 实体列表布局自动发现结束 =====

uintptr_t GetBaseEntity(int index, uintptr_t client) {
	if (!client) return 0;

	// 首次调用时自动发现布局
	if (!g_EntConfig.discovered)
		DiscoverEntityLayout(client);

	auto entSystem = *reinterpret_cast<std::uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwEntityList);
	if (entSystem == 0 || IsBadReadPtr(reinterpret_cast<void*>(entSystem), sizeof(uintptr_t)))
		return 0;

	int chunk = index >> 9;
	int slot  = index & 0x1FF;

	uintptr_t chunkPtrAddr = entSystem + 8ULL * chunk + g_EntConfig.chunkArrayOffset;
	if (IsBadReadPtr(reinterpret_cast<void*>(chunkPtrAddr), sizeof(uintptr_t)))
		return 0;

	uintptr_t chunkBase = *reinterpret_cast<uintptr_t*>(chunkPtrAddr);
	if (chunk == 0) chunkBase &= ~0x7ULL;
	if (!chunkBase) return 0;

	uintptr_t entityAddr = chunkBase + static_cast<uintptr_t>(slot) * g_EntConfig.entitySlotSize;
	if (IsBadReadPtr(reinterpret_cast<void*>(entityAddr), sizeof(uintptr_t)))
		return 0;

	return *reinterpret_cast<uintptr_t*>(entityAddr);
}

uintptr_t GetBaseEntityFromHandle(uint32_t uHandle, uintptr_t client) {
	if (!client) return 0;

	// 从 CHandle 中提取实体索引 (低15位)
	const int nIndex = uHandle & 0x7FFF;

	// 直接使用索引从实体列表获取实体
	return GetBaseEntity(nIndex, client);
}

bool WorldToScreen(Vector3 pWorldPos, Vector3& pScreenPos, float* pMatrixPtr, const FLOAT pWinWidth, const FLOAT pWinHeight)
{
	float matrix2[4][4];

	memcpy(matrix2, pMatrixPtr, 16 * sizeof(float));

	const float mX{ pWinWidth / 2 };
	const float mY{ pWinHeight / 2 };

	const float w{
		matrix2[3][0] * pWorldPos.x +
		matrix2[3][1] * pWorldPos.y +
		matrix2[3][2] * pWorldPos.z +
		matrix2[3][3] };

	if (w < 0.65f) return false;

	const float x{
		matrix2[0][0] * pWorldPos.x +
		matrix2[0][1] * pWorldPos.y +
		matrix2[0][2] * pWorldPos.z +
		matrix2[0][3] };

	const float y{
		matrix2[1][0] * pWorldPos.x +
		matrix2[1][1] * pWorldPos.y +
		matrix2[1][2] * pWorldPos.z +
		matrix2[1][3] };

	pScreenPos.x = (mX + mX * x / w);
	pScreenPos.y = (mY - mY * y / w);
	pScreenPos.z = 0;

	return true;
}

Vector3 BonePos(uintptr_t addr, int32_t index)
{
	int32_t d = 32 * index;
	uintptr_t address{};
	address = *reinterpret_cast<uintptr_t*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
	if (!address)
	{
		return Vector3();
	}

	auto BoneArray = cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState + 0x80;
	address = *reinterpret_cast<uintptr_t*>(address + BoneArray);
	if (!address)
	{
		return Vector3();
	}
	return *reinterpret_cast<Vector3*>(address + d);
}

uintptr_t GetPlayerPawn(uintptr_t playerController, uintptr_t client) {
	if (!playerController || !client) return 0;

	// 安全检查：验证 playerController 可读
	if (IsBadReadPtr(reinterpret_cast<void*>(playerController), sizeof(uintptr_t)))
		return 0;

	// 从Controller获取Pawn句柄 (CS2实际使用CCSPlayerController)
	auto playerHPawn = *reinterpret_cast<uint32_t*>(
		playerController + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn
		);
	if (playerHPawn == 0xFFFFFFFF) return 0;

	// 通过句柄获取Pawn实体
	return GetBaseEntityFromHandle(playerHPawn, client);
}

void DrawLine(std::vector<Vector3> list, ImColor Color, float* Matrix)
{
	Vector3 drawpos;
	std::vector<Vector3>Drawlist{};
	for (int i = 0; i < list.size(); ++i)
	{

		if (!WorldToScreen(list[i], drawpos, Matrix, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y))
			continue;

		Drawlist.push_back(drawpos);

	}

	for (int i = 1; i < Drawlist.size(); ++i)
	{
		ImGui::GetBackgroundDrawList()->AddLine(ImVec2(Drawlist[i].x, Drawlist[i].y), ImVec2(Drawlist[i - 1].x, Drawlist[i - 1].y), Color);
	}
}

void Bone_Start(uintptr_t pawn, ImColor BoneColor, float* Matrix) {

	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::head));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::spine_2));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));

	DrawLine(BoneDrawList, BoneColor, Matrix);


	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn,  Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn,  Bone_Base::BoneIndex::arm_upper_L));
	BoneDrawList.push_back(BonePos(pawn,  Bone_Base::BoneIndex::arm_lower_L));
	BoneDrawList.push_back(BonePos(pawn,  Bone_Base::BoneIndex::hand_L));
	DrawLine(BoneDrawList, BoneColor, Matrix);



	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::neck_0));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_upper_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::arm_lower_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::hand_R));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_upper_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_lower_L));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::ankle_L));
	DrawLine(BoneDrawList, BoneColor, Matrix);

	BoneDrawList.clear();
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::pelvis));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_upper_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::leg_lower_R));
	BoneDrawList.push_back(BonePos(pawn, Bone_Base::BoneIndex::ankle_R));
	DrawLine(BoneDrawList, BoneColor, Matrix);

}

std::optional<Vector3> GetEyePos(uintptr_t addr)  noexcept {
	if (!addr) return std::nullopt;

	// 使用 CGameSceneNode::m_vecAbsOrigin (0xC8) 作为原点，更可靠
	uintptr_t pGameSceneNode = 0;
	if (!IsBadReadPtr(reinterpret_cast<void*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode), sizeof(uintptr_t))) {
		pGameSceneNode = *reinterpret_cast<uintptr_t*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
	}

	Vector3 origin;
	if (pGameSceneNode && !IsBadReadPtr(reinterpret_cast<void*>(pGameSceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin), sizeof(Vector3))) {
		origin = *reinterpret_cast<Vector3*>(pGameSceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
	} else if (!IsBadReadPtr(reinterpret_cast<void*>(addr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin), sizeof(Vector3))) {
		origin = *reinterpret_cast<Vector3*>(addr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
	} else {
		return std::nullopt;
	}

	// m_vecViewOffset 在 C_BaseModelEntity 中，类型为 CNetworkViewOffsetVector
	// CNetworkViewOffsetVector 结构: 前12字节是 Vector (view offset)
	Vector3 viewOffset(0, 0, 64.0f); // 默认眼睛高度
	if (!IsBadReadPtr(reinterpret_cast<void*>(addr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset), sizeof(Vector3))) {
		viewOffset = *reinterpret_cast<Vector3*>(addr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
	}

	Vector3 LocalEye = origin + viewOffset;
	if (!std::isfinite(LocalEye.x) || !std::isfinite(LocalEye.y) || !std::isfinite(LocalEye.z))
		return std::nullopt;

	if (LocalEye.Length() < 0.1f)
		return std::nullopt;

	return LocalEye;
}

uintptr_t GetLocalPlayerPawn(uintptr_t client) {
	// 优先使用 dwLocalPlayerPawn 直接获取 (更快更可靠)
	auto localPawn = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
	if (localPawn && !IsBadReadPtr(reinterpret_cast<void*>(localPawn), sizeof(uintptr_t))) {
		return localPawn;
	}

	// 回退：通过 Controller 获取 Pawn
	auto localCtrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
	if (!localCtrl) return 0;

	auto localHPawn = *reinterpret_cast<uint32_t*>(localCtrl + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
	return GetBaseEntityFromHandle(localHPawn, client);
}

float GetDistanceToCrosshair(const Vector3& screenPos) {
	Vector2 crosshairPos(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2);
	return sqrt(
		pow(screenPos.x - crosshairPos.x, 2) +
		pow(screenPos.y - crosshairPos.y, 2)
	);
}

void CalculateTargetAngles(const Vector3& localPos, Vector3& targetPos, float& yaw, float& pitch) {
	Vector3 delta = targetPos - localPos;

	// 精确计算水平距离（忽略Z轴）
	float horizontalDistance = sqrt(delta.x * delta.x + delta.y * delta.y);

	// Yaw计算（水平角度）
	yaw = atan2f(delta.y, delta.x) * (180.0f / PI);

	// 修正Pitch计算（解决偏高问题）
	pitch = -atan2f(delta.z, horizontalDistance) * (180.0f / PI);

	// 角度规范化处理
	yaw = fmodf(yaw + 180.0f, 360.0f) - 180.0f;  // 保证在[-180,180]范围内
	if (pitch > 89.0f) pitch = 89.0f;
	else if (pitch < -89.0f) pitch = -89.0f; // 限制在[-89,89]之间
}

void MouseAim(const Vector3& targetScreenPos, float smoothFactor = 10.0f) {
	POINT currentPos;
	GetCursorPos(&currentPos);

	// 计算需要移动的像素距离（带平滑）
	float deltaX = (targetScreenPos.x - currentPos.x) / smoothFactor;
	float deltaY = (targetScreenPos.y - currentPos.y) / smoothFactor;

	// 添加微小随机扰动（反作弊规避）
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	deltaX += dist(gen);
	deltaY += dist(gen);

	// 发送鼠标输入
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;
	input.mi.dwFlags = MOUSEEVENTF_MOVE;
	input.mi.dx = static_cast<LONG>(deltaX);
	input.mi.dy = static_cast<LONG>(deltaY);
	SendInput(1, &input, sizeof(INPUT));
}

bool IsVisible(uintptr_t enemyPawn, uintptr_t localPawn) {
	// 获取本地玩家眼睛位置
	auto localEyePos = GetEyePos(localPawn);
	if (!localEyePos.has_value()) return false;

	// 获取敌人瞄准部位位置
	Vector3 targetPos;
	if (zeroflick::aim::aimHead) {
		targetPos = BonePos(enemyPawn, Bone_Base::BoneIndex::head);
	}
	else if (zeroflick::aim::aimBody) {
		targetPos = BonePos(enemyPawn, Bone_Base::BoneIndex::spine_1);
	}
	else if (zeroflick::aim::aimDick) {
		targetPos = BonePos(enemyPawn, Bone_Base::BoneIndex::pelvis);
	}
	else {
		return false;
	}

	// 简单的距离检查（可选）
	float distance = (targetPos - localEyePos.value()).Length();
	if (distance > 5000.0f) return false; // 超出最大可见距离

	// TODO: 这里需要实现实际的射线检测
	// 伪代码示例：
	// if (RayCast(localEyePos.value(), targetPos)) {
	//     return true;
	// }

	return true; // 暂时默认可见
}

// 增强的目标有效性检查
bool IsValidTarget(uintptr_t pawn, uintptr_t localPawn) {
	// 存活检查
	if (*reinterpret_cast<int*>(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth) <= 0)
		return false;

	// 队友检查
	uint8_t team = *reinterpret_cast<uint8_t*>(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
	uint8_t localTeam = *reinterpret_cast<uint8_t*>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
	if (team == localTeam)
		return false;

	// 可见性检查
	if (!IsVisible(pawn, localPawn))
		return false;

	return true;
}

// 修正后的目标选择逻辑
uintptr_t FindBestTarget(uintptr_t localPawn, uintptr_t client, float* matrix) {
	uintptr_t bestTarget = 0;
	float minCrosshairDistance = FLT_MAX;
	const float maxAimDistance = 10000.0f; // 最大瞄准距离

	Vector3 localEyePos = GetEyePos(localPawn).value();

	for (int i = 0; i < 64; i++) {
		auto player = GetBaseEntity(i, client);
		if (!player) continue;

		auto pawn = GetPlayerPawn(player, client);
		if (!pawn || pawn == localPawn) continue;

		if (!IsValidTarget(pawn, localPawn)) continue;

		// 根据配置选择瞄准部位
		Vector3 targetPos;
		if (zeroflick::aim::aimPart == 0) {
			targetPos = BonePos(pawn, Bone_Base::BoneIndex::head);
		}
		else if (zeroflick::aim::aimPart == 1) {
			targetPos = BonePos(pawn, Bone_Base::BoneIndex::spine_1);
		}
		else if (zeroflick::aim::aimPart == 2) {
			targetPos = BonePos(pawn, Bone_Base::BoneIndex::pelvis);
		}
		else {
			continue; // 无瞄准部位选择
		}

		// 检查目标是否在FOV范围内
		Vector3 screenPos;
		if (!WorldToScreen(targetPos, screenPos, matrix,
			ImGui::GetIO().DisplaySize.x,
			ImGui::GetIO().DisplaySize.y)) {
			continue;
		}

		float crosshairDistance = GetDistanceToCrosshair(screenPos);
		float actualDistance = (targetPos - localEyePos).Length();

		// 检查目标是否可见（无遮挡）
		if (!IsVisible(pawn, localPawn)) continue;

		// 检查是否在FOV范围内
		if (crosshairDistance > zeroflick::aim::FOVSize) continue;

		if (crosshairDistance < minCrosshairDistance && actualDistance <= maxAimDistance) {
			minCrosshairDistance = crosshairDistance;
			bestTarget = pawn;
		}
	}
	return bestTarget;
}

static const std::unordered_map<std::string, std::string> WEAPON_NAME_MAP = {
	{"weapon_hkp2000", "P2000"},
	{"weapon_deagle", "Desert Eagle"},
	{"weapon_elite", "Dual Berettas"},
	{"weapon_fiveseven", "Five-SeveN"},
	{"weapon_glock", "Glock-18"},
	{"weapon_ak47", "AK-47"},
	{"weapon_aug", "AUG"},
	{"weapon_awp", "AWP"},
	{"weapon_famas", "FAMAS"},
	{"weapon_g3sg1", "G3SG1"},
	{"weapon_galilar", "Galil AR"},
	{"weapon_m249", "M249"},
	{"weapon_m4a1", "M4A4"},
	{"weapon_mac10", "MAC-10"},
	{"weapon_p90", "P90"},
	{"weapon_ump45", "UMP-45"},
	{"weapon_xm1014", "XM1014"},
	{"weapon_bizon", "PP-Bizon"},
	{"weapon_mag7", "MAG-7"},
	{"weapon_negev", "Negev"},
	{"weapon_sawedoff", "Sawed-Off"},
	{"weapon_tec9", "Tec-9"},
	{"weapon_taser", "Zeus x27"},
	{"weapon_mp7", "MP7"},
	{"weapon_mp9", "MP9"},
	{"weapon_nova", "Nova"},
	{"weapon_p250", "P250"},
	{"weapon_scar20", "SCAR-20"},
	{"weapon_sg556", "SG 553"},
	{"weapon_ssg08", "SSG 08"},
	{"weapon_knife", "Knife"},
	{"weapon_flashbang", "Flashbang"},
	{"weapon_hegrenade", "HE Grenade"},
	{"weapon_smokegrenade", "Smoke Grenade"},
	{"weapon_molotov", "Molotov"},
	{"weapon_decoy", "Decoy Grenade"},
	{"weapon_incgrenade", "Incendiary Grenade"},
	{"weapon_c4", "C4 Explosive"},
	{"weapon_usp_silencer", "USP-S"},
	{"weapon_cz75a", "CZ75-Auto"},
	{"weapon_revolver", "R8 Revolver"},
	{"weapon_knife_t", "Knife (T)"},
	{"weapon_m4a1_silencer", "M4A1-S"},
	{"weapon_fists", "Fists"},
	{"weapon_breachcharge", "Breach Charge"},
	{"weapon_tablet", "Tablet"},
	{"weapon_melee", "Melee"},
	{"weapon_axe", "Axe"},
	{"weapon_hammer", "Hammer"},
	{"weapon_spanner", "Wrench"},
	{"weapon_knife_ghost", "Ghost Knife"},
	{"weapon_firebomb", "Fire Bomb"},
	{"weapon_diversion", "Diversion Device"},
	{"weapon_frag_grenade", "Frag Grenade"},
	{"weapon_snowball", "Snowball"},
	{"weapon_bumpmine", "Bump Mine"},
	{"weapon_bayonet", "Bayonet"},
	{"weapon_knife_flip", "Flip Knife"},
	{"weapon_knife_gut", "Gut Knife"},
	{"weapon_knife_karambit", "Karambit"},
	{"weapon_knife_m9_bayonet", "M9 Bayonet"},
	{"weapon_knife_tactical", "Huntsman Knife"},
	{"weapon_knife_falchion", "Falchion Knife"},
	{"weapon_knife_survival_bowie", "Bowie Knife"},
	{"weapon_knife_butterfly", "Butterfly Knife"},
	{"weapon_knife_push", "Shadow Daggers"},
	{"weapon_knife_cord", "Paracord Knife"},
	{"weapon_knife_canis", "Survival Knife"},
	{"weapon_knife_ursus", "Ursus Knife"},
	{"weapon_knife_gypsy_jackknife", "Navaja Knife"},
	{"weapon_knife_outdoor", "Nomad Knife"},
	{"weapon_knife_stiletto", "Stiletto Knife"},
	{"weapon_knife_widowmaker", "Talon Knife"},
	{"weapon_knife_skeleton", "Skeleton Knife"}
};




void draw_esp() {
	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	if (!client) return;

	auto local_ctrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
	if (!local_ctrl) return;

	auto localPawn = GetLocalPlayerPawn(client);
	if (!localPawn) return;

	auto localteam = *reinterpret_cast<uint8_t*>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

	auto Matrix = reinterpret_cast<float*>(client + cs2_dumper::offsets::client_dll::dwViewMatrix);
	if (!Matrix || IsBadReadPtr(Matrix, 16 * sizeof(float))) return;

	// 1. 先寻找最佳目标（仅在FOV范围内）
	uintptr_t bestTarget = FindBestTarget(localPawn, client, Matrix);
	Vector3 bestTargetPos;

	if (bestTarget) {
		// 根据配置选择瞄准部位
		if (zeroflick::aim::aimPart == 0) {
			bestTargetPos = BonePos(bestTarget, Bone_Base::BoneIndex::head);
		}
		else if (zeroflick::aim::aimPart == 1) {
			bestTargetPos = BonePos(bestTarget, Bone_Base::BoneIndex::spine_1);
		}
		else if (zeroflick::aim::aimPart == 2) {
			bestTargetPos = BonePos(bestTarget, Bone_Base::BoneIndex::pelvis);
		}

		// 计算屏幕坐标
		Vector3 bestTargetScreenPos;
		if (WorldToScreen(bestTargetPos, bestTargetScreenPos, Matrix,
			ImGui::GetIO().DisplaySize.x,
			ImGui::GetIO().DisplaySize.y)) {

			// 自瞄逻辑（只在目标可见时生效）
			if (GetAsyncKeyState(zeroflick::aim::aimKey) &&
				zeroflick::aim::aimbot &&
				IsVisible(bestTarget, localPawn)) {

				MouseAim(bestTargetScreenPos, zeroflick::aim::smoothFactorValue);
			}
		}
	}




	// 2. 独立绘制所有可见敌人（不限制FOV）
	  // 添加一个总开关控制ESP绘制
		for (int i = 0; i < 64; i++) {
			auto player_co = GetBaseEntity(i, client); //player_co 存储了玩家控制器(CBasePlayerController)的基地址
			if (!player_co) continue;

			auto player_hpawn = *reinterpret_cast<uint32_t*>(player_co + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
			if (player_hpawn == 0xFFFFFFFF) continue;//从玩家控制器中读取m_hPawn成员（存储的是玩家Pawn实体的句柄）

			auto player_pawn = GetBaseEntityFromHandle(player_hpawn, client);//player_pawn存储了玩家Pawn(CBasePlayerPawn)的基地址
			if (!player_pawn) continue;

			auto player_team = *reinterpret_cast<uint8_t*>(player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
			if (localteam == player_team) continue;

			auto player_health = *reinterpret_cast<int*>(player_pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
			if (player_health <= 0) continue;

			auto player_Origin = *reinterpret_cast<Vector3*>(player_pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);

			auto player_eyepos_op_vec = GetEyePos(player_pawn);
			if (!player_eyepos_op_vec.has_value()) continue;
			auto player_eyepos = player_eyepos_op_vec.value();

			auto player_name_ptr = reinterpret_cast<char*>(player_co + cs2_dumper::schemas::client_dll::CBasePlayerController::m_iszPlayerName);
			if (!player_name_ptr) continue;
			std::string player_name(player_name_ptr); // 转换为 std::string

			// 1. 获取武器服务指针
			uintptr_t weaponServices = 0;
			if (!IsBadReadPtr(reinterpret_cast<void*>(player_pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices), sizeof(uintptr_t))) {
				weaponServices = *reinterpret_cast<uintptr_t*>(player_pawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices);
			}

			// 2. 获取当前活跃武器
			std::string weapon_name = "Unknown";
			if (weaponServices && !IsBadReadPtr(reinterpret_cast<void*>(weaponServices), sizeof(uintptr_t))) {
				uint32_t activeWeaponHandle = 0;
				if (!IsBadReadPtr(reinterpret_cast<void*>(weaponServices + cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon), sizeof(uint32_t))) {
					activeWeaponHandle = *reinterpret_cast<uint32_t*>(weaponServices + cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon);
				}

				if (activeWeaponHandle != 0 && activeWeaponHandle != 0xFFFFFFFF) {
					uintptr_t weapon_entity = GetBaseEntityFromHandle(activeWeaponHandle, client);
					if (weapon_entity && !IsBadReadPtr(reinterpret_cast<void*>(weapon_entity), sizeof(uintptr_t))) {
						// 获取 CEntityIdentity* (偏移 0x10)
						uintptr_t entityIdentity = 0;
						if (!IsBadReadPtr(reinterpret_cast<void*>(weapon_entity + 0x10), sizeof(uintptr_t))) {
							entityIdentity = *reinterpret_cast<uintptr_t*>(weapon_entity + 0x10);
						}

						if (entityIdentity && !IsBadReadPtr(reinterpret_cast<void*>(entityIdentity), sizeof(uintptr_t))) {
							// m_designerName 在 CEntityIdentity 中的偏移是 0x20, 类型是 CUtlSymbolLarge
							// CUtlSymbolLarge: 前8字节是指向字符串的指针
							const char* designer_name_ptr = nullptr;
							if (!IsBadReadPtr(reinterpret_cast<void*>(entityIdentity + cs2_dumper::schemas::client_dll::CEntityIdentity::m_designerName), sizeof(const char*))) {
								designer_name_ptr = *reinterpret_cast<const char**>(entityIdentity + cs2_dumper::schemas::client_dll::CEntityIdentity::m_designerName);
							}

							if (designer_name_ptr && !IsBadStringPtrA(designer_name_ptr, 256)) {
								try {
									weapon_name = std::string(designer_name_ptr);
									auto it = WEAPON_NAME_MAP.find(weapon_name);
									if (it != WEAPON_NAME_MAP.end()) {
										weapon_name = it->second;
									} else if (weapon_name.find("weapon_") == 0) {
										weapon_name = weapon_name.substr(7);
									}
								} catch (...) {
									weapon_name = "Error";
								}
							}
						}
					}
				}
			}
			
			static const float w = ImGui::GetIO().DisplaySize.x;
			static const float h = ImGui::GetIO().DisplaySize.y;

			Vector3 head_pos_2d{};
			Vector3 abs_pos_2d{};

			if (!WorldToScreen(player_Origin, abs_pos_2d, Matrix, w, h)) continue;
			if (!WorldToScreen(player_eyepos, head_pos_2d, Matrix, w, h)) continue;

			const float height{ ::abs(head_pos_2d.y - abs_pos_2d.y) * 1.25f };
			const float width{ height / 2.f };
			const float x = head_pos_2d.x - (width / 2.f);
			const float y = head_pos_2d.y - (width / 2.5f);

			if (zeroflick::visuals::name) {
				ImVec2 nameTextSize = ImGui::CalcTextSize(player_name.c_str());
				float fontSize = std::clamp(width * 0.1f, 12.0f, 20.0f);
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
				ImGui::GetBackgroundDrawList()->AddText(
					ImGui::GetIO().Fonts->Fonts[0],
					fontSize,
					ImVec2(x + (width - nameTextSize.x) * 0.5f, y - nameTextSize.y - 2.0f),
					ImColor(zeroflick::visuals::namecol[0], zeroflick::visuals::namecol[1], zeroflick::visuals::namecol[2], zeroflick::visuals::namecol[3]),
					player_name.c_str()
				);
				ImGui::PopFont();
			}


			if (zeroflick::visuals::weapon) {
				ImVec2 weaponTextSize = ImGui::CalcTextSize(weapon_name.c_str());
				float fontSize = std::clamp(width * 0.1f, 12.0f, 20.0f);
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
				ImGui::GetBackgroundDrawList()->AddText(
					ImGui::GetIO().Fonts->Fonts[0],
					fontSize,
					ImVec2(x + (width - weaponTextSize.x) * 0.5f, y + height + 5.0f),
					ImColor(zeroflick::visuals::weaponcol[0], zeroflick::visuals::weaponcol[1], zeroflick::visuals::weaponcol[2], zeroflick::visuals::weaponcol[3]),
					weapon_name.c_str()
				);
				ImGui::PopFont();
			}

			if (zeroflick::visuals::box)
				ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), ImColor(zeroflick::visuals::BOXcol[0], zeroflick::visuals::BOXcol[1], zeroflick::visuals::BOXcol[2], zeroflick::visuals::BOXcol[3]), 0.0f, 0, 1.0f);

			if (zeroflick::visuals::hp) {
				std::string healthText = std::to_string(player_health) + " HP";
				ImVec2 healthTextSize = ImGui::CalcTextSize(healthText.c_str());

				// 动态字体大小（与名字一致）
				float fontSize = std::clamp(width * 0.1f, 12.0f, 20.0f);
				ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
				ImGui::GetBackgroundDrawList()->AddText(
					ImGui::GetIO().Fonts->Fonts[0],
					fontSize,
					ImVec2(x + width + 5.0f, y), // 右侧外部，间距5px
					ImColor(zeroflick::visuals::HPcol[0], zeroflick::visuals::HPcol[1], zeroflick::visuals::HPcol[2], zeroflick::visuals::HPcol[3]),
					healthText.c_str()
				);
				ImGui::PopFont();
				// 血条背景（红色）
				const float healthBarWidth = 2.0f;
				const float healthBarOffset = 2.0f;
				ImGui::GetBackgroundDrawList()->AddRectFilled(
					ImVec2(x - healthBarOffset - healthBarWidth, y),
					ImVec2(x - healthBarOffset, y + height),
					ImColor(255, 0, 0, 150)
				);

				// 当前血量（绿色）
				float healthPercentage = static_cast<float>(player_health) / 100.0f;
				float healthHeight = height * healthPercentage;
				ImGui::GetBackgroundDrawList()->AddRectFilled(
					ImVec2(x - healthBarOffset - healthBarWidth, y + height - healthHeight),
					ImVec2(x - healthBarOffset, y + height),
					ImColor(0, 255, 0, 200)
				);
			}

			if (zeroflick::visuals::bone)
				Bone_Start(player_pawn, ImColor(zeroflick::visuals::Bonecol[0], zeroflick::visuals::Bonecol[1], zeroflick::visuals::Bonecol[2], zeroflick::visuals::Bonecol[3]), Matrix);

			// 如果是当前目标，绘制特殊标记
			if (player_pawn == bestTarget && zeroflick::aim::inspectEn) {
				ImGui::GetBackgroundDrawList()->AddCircle(
					ImVec2(head_pos_2d.x, head_pos_2d.y),
					zeroflick::aim::inspectEnSize,
					ImColor(255, 255, 255, 255)
				);
			}
		}
	

	// 绘制FOV圆圈（仅用于视觉参考）
	if (zeroflick::aim::fov) {
		ImGui::GetBackgroundDrawList()->AddCircle(
			ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
			zeroflick::aim::FOVSize,
			ImColor(255, 255, 255, 255), 0, 0.5f
		);
	}
}






