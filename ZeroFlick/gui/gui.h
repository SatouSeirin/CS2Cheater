#pragma once
#define WIN32_LEAN_AND_MEAN
#define IMGUI_DEFINE_MATH_OPERATORS
#include <Windows.h>
#include <d3d11.h>
#include "../imgui_d11/imgui.h"
#include "../imgui_d11/imgui_internal.h"
#include "neverlose_gui.hpp"
#include "hashes.hpp"
#include <string>
#include <vector>

// ═══════════════════════════════════════════════
//  字体 (neverlose + 保留)
// ═══════════════════════════════════════════════
extern ImFont* g_NLMainFont;      // museo500, 14px
extern ImFont* g_NLTitleFont;     // museo900, 28px
extern ImFont* g_NLIconFont;      // font_awesome merged
extern ImFont* LexendDecaFont;
extern ImFont* InterMedium;

// ═══════════════════════════════════════════════
//  菜单状态
// ═══════════════════════════════════════════════
extern int g_menu_tab;

// ═══════════════════════════════════════════════
//  ZeroFlick 功能变量
// ═══════════════════════════════════════════════
namespace zeroflick {
	namespace visuals {
		extern bool  box, bone, name, hp, weapon, snapline, distance;
		extern bool  visible_only, team_check;
		extern float BOXcol[4], Bonecol[4], namecol[4], HPcol[4];
		extern float weaponcol[4], snaplinecol[4];
		extern float maxDist;
	}
	namespace aim {
		extern bool  aimbot, autoaim, autopunch, inspectEn, fov;
		extern float FOVSize, autopunchsenx, autopunchseny;
		extern float smoothFactorValue, inspectEnSize;
		extern int   aimKey, triggerKey, aimPart;
		extern bool  aimHead, aimBody, aimDick;
	}
	namespace movement {
		extern bool  bhop;
	}
	namespace misc {
		extern bool  auto_strafe, no_flash;
		extern float flash_alpha;
	}
}

// ═══════════════════════════════════════════════
//  初始化 & 渲染
// ═══════════════════════════════════════════════
void gui_Initialize();
void draw_Menu();

// ═══════════════════════════════════════════════
//  反馈系统
// ═══════════════════════════════════════════════
struct Feedback {
	std::string message;
	float timer = 0.0f;
	ImVec4 color = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
};
extern Feedback g_feedback;
void ShowFeedback();
void SetFeedback(const std::string& msg, const ImVec4& col = ImVec4(0.26f, 0.59f, 0.98f, 1.0f), float dur = 2.0f);

// ═══════════════════════════════════════════════
//  配置管理
// ═══════════════════════════════════════════════
void SaveCurrentConfig(const std::string& filename);
void LoadConfig(const std::string& filename);
void RefreshConfigList();
