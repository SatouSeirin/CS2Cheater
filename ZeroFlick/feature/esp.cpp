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


uintptr_t GetBaseEntity(int index, uintptr_t client) {
	if (!client) return 0;

	// CS2 实体列表结构 (从内存dump验证):
	// entSystem = *dwEntityList  (CGameEntitySystem*)
	// entSystem+0x10 -> 实体数组基址 (低位可能有标志位, 需要 & ~0x7)
	// 每个实体槽位 0x70 (112 字节)
	// 实体指针 = *(entSystem+0x10 & ~0x7 + index * 0x70)

	auto entSystem = *reinterpret_cast<std::uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwEntityList);
	if (entSystem == 0) return 0;
	if (IsBadReadPtr(reinterpret_cast<void*>(entSystem), sizeof(uintptr_t)))
		return 0;

	// 从 entSystem+0x10 读取实体数组基址, 清除低3位标志
	uintptr_t entityArrayBase = 0;
	if (IsBadReadPtr(reinterpret_cast<void*>(entSystem + 0x10), sizeof(uintptr_t)))
		return 0;

	entityArrayBase = *reinterpret_cast<uintptr_t*>(entSystem + 0x10);
	entityArrayBase &= ~0x7ULL; // 清除可能的标志位

	if (!entityArrayBase)
		return 0;

	const int ENTITY_SIZE = 0x70; // 每个实体槽位 112 字节

	uintptr_t entityAddr = entityArrayBase + index * ENTITY_SIZE;
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
	if (!addr) return Vector3();

	// CS2 骨骼获取路径:
	// C_BaseEntity + m_pGameSceneNode (0x330) -> CGameSceneNode* (即 CSkeletonInstance*)
	// CSkeletonInstance + 0x1F0 -> pBoneCache (Matrix2x4_t*)
	// 每个骨骼矩阵是 32 字节 (4个Vector4D: [4x4]中的3行 + padding)
	// 骨骼位置在矩阵的第4列: offset = index * 32 + 12 (第4个float)

	uintptr_t pGameSceneNode = 0;
	if (IsBadReadPtr(reinterpret_cast<void*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode), sizeof(uintptr_t)))
		return Vector3();

	pGameSceneNode = *reinterpret_cast<uintptr_t*>(addr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
	if (!pGameSceneNode)
		return Vector3();

	// pBoneCache 在 CSkeletonInstance 中的偏移 (asphyxia-cs2 验证: 0x1F0)
	constexpr std::ptrdiff_t pBoneCache = 0x1F0;

	uintptr_t boneMatrix = 0;
	if (IsBadReadPtr(reinterpret_cast<void*>(pGameSceneNode + pBoneCache), sizeof(uintptr_t)))
		return Vector3();

	boneMatrix = *reinterpret_cast<uintptr_t*>(pGameSceneNode + pBoneCache);
	if (!boneMatrix)
		return Vector3();

	// 每个骨骼矩阵是 Matrix2x4_t = 32 字节 (2行 x 4列 = 8个float)
	// 骨骼位置在矩阵中的偏移取决于具体实现
	// asphyxia-cs2 的 CBoneData: {Vector3 Location, float Scale, Quaternion Rotation} = 32字节
	// 即 boneMatrix + index * 32 读取前12字节即可得到位置
	uintptr_t boneAddr = boneMatrix + index * 32;
	if (IsBadReadPtr(reinterpret_cast<void*>(boneAddr), sizeof(Vector3)))
		return Vector3();

	return *reinterpret_cast<Vector3*>(boneAddr);
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




// 最简调试: 每个 early return 前打印失败原因
// 设为 false 让每次调用都打印 (用于调试)
static bool debugOnce = false;

void draw_esp() {
	printf("[ZeroFlick DEBUG] ===== draw_esp() called =====\n");

	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	if (!client) {
		printf("[ZeroFlick DEBUG] FAIL: client.dll not found!\n");
		return;
	}
	printf("[ZeroFlick DEBUG] client.dll: 0x%p\n", (void*)client);

	auto local_ctrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
	if (!local_ctrl) {
		printf("[ZeroFlick DEBUG] FAIL: local_ctrl is NULL (offset dwLocalPlayerController = 0x%X)\n",
			cs2_dumper::offsets::client_dll::dwLocalPlayerController);
		return;
	}
	printf("[ZeroFlick DEBUG] local_ctrl: 0x%p\n", (void*)local_ctrl);

	auto localPawn = GetLocalPlayerPawn(client);
	if (!localPawn) {
		printf("[ZeroFlick DEBUG] FAIL: localPawn is NULL! (m_hPlayerPawn offset = 0x%X)\n",
			cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
		return;
	}
	printf("[ZeroFlick DEBUG] localPawn: 0x%p\n", (void*)localPawn);

	auto localteam = *reinterpret_cast<uint8_t*>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
	printf("[ZeroFlick DEBUG] localteam: %d\n", localteam);

	// dwViewMatrix 直接指向 float[16] 视图矩阵，不需要二次解引用
	auto Matrix = reinterpret_cast<float*>(client + cs2_dumper::offsets::client_dll::dwViewMatrix);
	if (!Matrix || IsBadReadPtr(Matrix, 16 * sizeof(float))) {
		printf("[ZeroFlick DEBUG] FAIL: Matrix invalid at 0x%p!\n", (void*)Matrix);
		return;
	}
	printf("[ZeroFlick DEBUG] ViewMatrix: 0x%p, [0]=%.4f [1]=%.4f [2]=%.4f [3]=%.4f\n",
		(void*)Matrix, Matrix[0], Matrix[1], Matrix[2], Matrix[3]);

	// 验证 GetBaseEntity 修复
	{
		uint32_t localHpawn = *reinterpret_cast<uint32_t*>(local_ctrl + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
		int localIndex = localHpawn & 0x7FFF;
		printf("[ZeroFlick DEBUG] local m_hPlayerPawn=0x%X, index=%d, localPawn=0x%p\n",
			localHpawn, localIndex, (void*)localPawn);

		uintptr_t testResult = GetBaseEntity(localIndex, client);
		printf("[ZeroFlick DEBUG] GetBaseEntity(%d) = 0x%p, match=%s\n",
			localIndex, (void*)testResult,
			(testResult == localPawn) ? "YES!!" : "NO");

		if (testResult != localPawn) {
			printf("[ZeroFlick DEBUG] ===== GetBaseEntity still broken, stopping =====\n");
			return;
		}
	}

	// GetBaseEntity 已验证正确，继续执行完整 ESP
	// (如果上面 return 了说明还没修好，不会执行到这里)
	printf("[ZeroFlick DEBUG] ===== GetBaseEntity OK, running full ESP =====\n");

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






