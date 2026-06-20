#pragma once
#include "../imgui_d11/imgui.h"
#include "../utils/Vector.h"
#include <vector>
uintptr_t GetBaseEntity(int index, uintptr_t client);
uintptr_t GetBaseEntityFromHandle(uint32_t uHandle, uintptr_t client);
void draw_esp();

namespace Bone_Base {

	enum BoneIndex {
		head = 6,           // 头部
		neck_0 = 5,         // 颈部
		spine_1 = 4,        // 脊椎1
		spine_2 = 2,        // 脊椎2
		pelvis = 0,         // 骨盆
		arm_upper_L = 8,    // 左上臂
		arm_lower_L = 9,    // 左前臂
		hand_L = 10,        // 左手
		arm_upper_R = 13,   // 右上臂
		arm_lower_R = 14,   // 右前臂
		hand_R = 15,        // 右手
		leg_upper_L = 22,   // 左大腿
		leg_lower_L = 23,   // 左小腿
		ankle_L = 24,       // 左脚踝
		leg_upper_R = 25,   // 右大腿
		leg_lower_R = 26,   // 右小腿
		ankle_R = 27,       // 右脚踝
	};


}


//取骨骼坐标
Vector3 BonePos(uintptr_t addr, int32_t index);
//全身骨骼绘制
void Bone_Start(uintptr_t pawn, ImColor BoneColor, float* Matrix);
//骨骼绘画列表的连线
void DrawLine(std::vector<Vector3> list, ImColor Color, float* Matrix);

inline std::vector<Vector3>BoneDrawList{};