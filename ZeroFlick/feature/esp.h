#pragma once
#include "../imgui_d11/imgui.h"
#include "../utils/Vector.h"
#include <vector>
uintptr_t GetBaseEntity(int index, uintptr_t client);
uintptr_t GetBaseEntityFromHandle(uint32_t uHandle, uintptr_t client);
void draw_esp();

namespace Bone_Base {

	enum BoneIndex {
		head = 7,           // 头部
		neck_0 = 6,         // 颈部
		spine_1 = 5,        // 脊柱1
		spine_2 = 3,        // 脊柱2
		pelvis = 2,         // 骨盆（或臀部）
		arm_upper_L = 9,    // 左上臂
		arm_lower_L = 10,    // 左前臂
		hand_L = 11,        // 左手
		arm_upper_R = 13,   // 右上臂
		arm_lower_R = 14,   // 右前臂
		hand_R = 15,        // 右手
		leg_upper_L = 17,   // 左大腿
		leg_lower_L = 18,   // 左小腿
		ankle_L = 19,       // 左脚踝
		leg_upper_R = 20,   // 右大腿
		leg_lower_R = 21,   // 右小腿
		ankle_R = 22,       // 右脚踝
		eye_foward = 24,    // 看的地方
	};


}


//ȡ��������
Vector3 BonePos(uintptr_t addr, int32_t index);
//ȫ����������
void Bone_Start(uintptr_t pawn, ImColor BoneColor, float* Matrix);
//�����滭�б�������
void DrawLine(std::vector<Vector3> list, ImColor Color, float* Matrix);

inline std::vector<Vector3>BoneDrawList{};