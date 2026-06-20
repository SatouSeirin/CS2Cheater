#define WIN32_LEAN_AND_MEAN
#define IMGUI_DEFINE_MATH_OPERATORS

#include <Windows.h>
#include <d3d11.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <filesystem>

#include "../imgui_d11/imgui.h"
#include "../imgui_d11/imgui_internal.h"

#include "color_t.hpp"
#include "neverlose_gui.hpp"
#include "hashes.hpp"
#include "bytes.hpp"

#include "./gui.h"

using namespace ImGui;
namespace fs = std::filesystem;

// ═══════════════════════════════════════════
//  ZeroFlick 功能变量定义
// ═══════════════════════════════════════════
namespace zeroflick {
	namespace visuals {
		bool  box       = false;   float BOXcol[4]    = { 0.0f, 1.0f, 0.82f, 1.0f };
		bool  bone      = false;   float Bonecol[4]   = { 1.0f, 1.0f, 1.0f,  1.0f };
		bool  name      = false;   float namecol[4]   = { 1.0f, 1.0f, 1.0f,  1.0f };
		bool  hp        = false;   float HPcol[4]     = { 0.0f, 1.0f, 0.0f,  1.0f };
		bool  weapon    = false;   float weaponcol[4] = { 1.0f, 1.0f, 0.0f,  1.0f };
		bool  snapline  = false;   float snaplinecol[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
		bool  distance  = false;
		float maxDist   = 300.f;
		bool  visible_only = false;
		bool  team_check   = false;
	}
	namespace aim {
		bool  aimbot    = true;
		bool  autoaim   = false;
		bool  autopunch = false;
		bool  inspectEn = false;
		bool  fov       = false;
		float FOVSize        = 100.0f;
		float autopunchsenx  = 1.0f;
		float autopunchseny  = 1.0f;
		float smoothFactorValue = 4.0f;
		float inspectEnSize  = 10.0f;
		int   aimKey     = VK_MENU;
		int   triggerKey = VK_SHIFT;
		int   aimPart    = 0;
		bool  aimHead    = true;
		bool  aimBody    = false;
		bool  aimDick    = false;
	}
	namespace movement {
		bool  bhop = false;
	}
	namespace misc {
		bool  auto_strafe = false;
		bool  no_flash    = false;
		float flash_alpha = 0.3f;
	}
}

// ═══════════════════════════════════════════
//  全局字体
// ═══════════════════════════════════════════
ImFont* g_NLMainFont    = nullptr;
ImFont* g_NLTitleFont   = nullptr;
ImFont* g_NLIconFont    = nullptr;
ImFont* LexendDecaFont  = nullptr;
ImFont* InterMedium     = nullptr;
ImFont* IconFontLogs    = nullptr;

int g_menu_tab = 0;

static bool isBindingAimKey     = false;
static bool isBindingTriggerKey = false;

// ═══════════════════════════════════════════
//  反馈系统
// ═══════════════════════════════════════════
Feedback g_feedback;

void ShowFeedback() {
	if (g_feedback.timer <= 0.0f) return;
	float alpha = ImMin(g_feedback.timer / 0.5f, 1.0f);
	PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
	PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.114f, 0.118f, 0.133f, 0.92f * alpha));
	ImVec2 ts = CalcTextSize(g_feedback.message.c_str());
	float ww = ts.x + 40.f, wh = ts.y + 28.f;
	SetNextWindowPos(ImVec2((GetIO().DisplaySize.x - ww) * 0.5f, GetIO().DisplaySize.y * 0.38f), ImGuiCond_Always);
	SetNextWindowSize(ImVec2(ww, wh));
	if (Begin("##Feedback", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs)) {
		SetCursorPos(ImVec2(20.0f, (wh - ts.y) * 0.5f));
		g_feedback.color.w = alpha;
		TextColored(g_feedback.color, "%s", g_feedback.message.c_str());
	}
	End();
	PopStyleColor();
	PopStyleVar(2);
	g_feedback.timer -= GetIO().DeltaTime;
}

void SetFeedback(const std::string& msg, const ImVec4& col, float dur) {
	g_feedback.message = msg; g_feedback.color = col; g_feedback.timer = dur;
}

// ═══════════════════════════════════════════
//  配置 I/O
// ═══════════════════════════════════════════
static std::vector<std::string> configList;
static int   selectedConfig = -1;
static char  newConfigName[64] = "";

static std::string Trim(const std::string& s) {
	size_t f = s.find_first_not_of(" \t\n\r");
	if (f == std::string::npos) return "";
	return s.substr(f, s.find_last_not_of(" \t\n\r") - f + 1);
}

void SaveCurrentConfig(const std::string& filename) {
	std::ofstream f(filename + ".ini");
	if (f.is_open()) {
		f << "[ESP]\nbox=" << zeroflick::visuals::box
		  << "\nbone=" << zeroflick::visuals::bone
		  << "\nhp=" << zeroflick::visuals::hp
		  << "\nname=" << zeroflick::visuals::name
		  << "\nweapon=" << zeroflick::visuals::weapon
		  << "\nsnapline=" << zeroflick::visuals::snapline
		  << "\ndistance=" << zeroflick::visuals::distance
		  << "\nmaxdist=" << zeroflick::visuals::maxDist
		  << "\nvisible_only=" << zeroflick::visuals::visible_only
		  << "\nteam_check=" << zeroflick::visuals::team_check
		  << "\n[AIM]\naimbot=" << zeroflick::aim::aimbot
		  << "\naim_key=" << zeroflick::aim::aimKey
		  << "\nsmooth=" << zeroflick::aim::smoothFactorValue
		  << "\nautoaim=" << zeroflick::aim::autoaim
		  << "\ntrigger_key=" << zeroflick::aim::triggerKey
		  << "\nautopunch=" << zeroflick::aim::autopunch
		  << "\nfov=" << zeroflick::aim::fov
		  << "\nfovsize=" << zeroflick::aim::FOVSize
		  << "\ninspectEn=" << zeroflick::aim::inspectEn
		  << "\ninspectEnSize=" << zeroflick::aim::inspectEnSize
		  << "\naimHead=" << zeroflick::aim::aimHead
		  << "\naimBody=" << zeroflick::aim::aimBody
		  << "\n[MISC]\nbhop=" << zeroflick::movement::bhop
		  << "\nno_flash=" << zeroflick::misc::no_flash
		  << "\nflash_alpha=" << zeroflick::misc::flash_alpha
		  << "\n";
		f.close();
		SetFeedback("Saved: " + filename, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	} else {
		SetFeedback("Save failed!", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	}
}

void LoadConfig(const std::string& filename) {
	std::ifstream f(filename + ".ini");
	if (!f.is_open()) { SetFeedback("Load failed!", ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); return; }
	std::string line, section;
	while (std::getline(f, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == ';') continue;
		if (line[0] == '[') { section = line.substr(1, line.find(']') - 1); continue; }
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
		auto b = [&] { return v == "1"; };
		try {
			int iv = std::stoi(v);
			float fv = std::stof(v);
			if (section == "ESP") {
				if (k == "box") zeroflick::visuals::box = b();
				else if (k == "bone") zeroflick::visuals::bone = b();
				else if (k == "hp") zeroflick::visuals::hp = b();
				else if (k == "name") zeroflick::visuals::name = b();
				else if (k == "weapon") zeroflick::visuals::weapon = b();
				else if (k == "snapline") zeroflick::visuals::snapline = b();
				else if (k == "distance") zeroflick::visuals::distance = b();
				else if (k == "maxdist") zeroflick::visuals::maxDist = fv;
				else if (k == "visible_only") zeroflick::visuals::visible_only = b();
				else if (k == "team_check") zeroflick::visuals::team_check = b();
			} else if (section == "AIM") {
				if (k == "aimbot") zeroflick::aim::aimbot = b();
				else if (k == "aim_key") zeroflick::aim::aimKey = iv;
				else if (k == "smooth") zeroflick::aim::smoothFactorValue = fv;
				else if (k == "autoaim") zeroflick::aim::autoaim = b();
				else if (k == "trigger_key") zeroflick::aim::triggerKey = iv;
				else if (k == "autopunch") zeroflick::aim::autopunch = b();
				else if (k == "fov") zeroflick::aim::fov = b();
				else if (k == "fovsize") zeroflick::aim::FOVSize = fv;
				else if (k == "inspectEn") zeroflick::aim::inspectEn = b();
				else if (k == "inspectEnSize") zeroflick::aim::inspectEnSize = fv;
				else if (k == "aimHead") zeroflick::aim::aimHead = b();
				else if (k == "aimBody") zeroflick::aim::aimBody = b();
			} else if (section == "MISC") {
				if (k == "bhop") zeroflick::movement::bhop = b();
				else if (k == "no_flash") zeroflick::misc::no_flash = b();
				else if (k == "flash_alpha") zeroflick::misc::flash_alpha = fv;
			}
		} catch (...) {}
	}
	f.close();
	SetFeedback("Loaded: " + filename, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
}

void RefreshConfigList() {
	configList.clear();
	for (const auto& e : fs::directory_iterator("."))
		if (e.path().extension() == ".ini")
			configList.push_back(e.path().stem().string());
	std::sort(configList.begin(), configList.end());
}

// ═══════════════════════════════════════════
//  gui_Initialize — neverlose 暗黑主题
// ═══════════════════════════════════════════
void gui_Initialize() {
	RefreshConfigList();

	ImGuiStyle& s = GetStyle();
	s.WindowPadding   = ImVec2(0, 0);
	s.FrameRounding   = 6;
	s.ChildRounding   = 10;
	s.PopupRounding   = 5;
	s.GrabRounding    = 4;
	s.ScrollbarRounding = 4;

	s.Colors[ImGuiCol_WindowBg]           = ImVec4(0.043f, 0.047f, 0.058f, 1.0f);
	s.Colors[ImGuiCol_ChildBg]            = ImVec4(0.019f, 0.023f, 0.035f, 1.0f);
	s.Colors[ImGuiCol_PopupBg]            = ImVec4(0.019f, 0.023f, 0.035f, 1.0f);
	s.Colors[ImGuiCol_Text]               = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
	s.Colors[ImGuiCol_TextDisabled]       = ImVec4(0.51f, 0.52f, 0.56f, 1.0f);
	s.Colors[ImGuiCol_Border]             = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
	s.Colors[ImGuiCol_FrameBg]            = ImVec4(0.023f, 0.039f, 0.07f, 1.0f);
	s.Colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.043f, 0.07f, 0.137f, 1.0f);
	s.Colors[ImGuiCol_FrameBgActive]      = ImVec4(0.043f, 0.07f, 0.137f, 1.0f);
	s.Colors[ImGuiCol_TitleBg]            = ImVec4(0.019f, 0.023f, 0.035f, 1.0f);
	s.Colors[ImGuiCol_TitleBgActive]      = ImVec4(0.019f, 0.023f, 0.035f, 1.0f);
	s.Colors[ImGuiCol_Button]             = ImVec4(0.031f, 0.035f, 0.058f, 1.0f);
	s.Colors[ImGuiCol_ButtonHovered]      = ImVec4(0.050f, 0.054f, 0.078f, 1.0f);
	s.Colors[ImGuiCol_ButtonActive]       = ImVec4(0.07f, 0.074f, 0.098f, 1.0f);
	s.Colors[ImGuiCol_Header]             = ImVec4(0.023f, 0.039f, 0.07f, 1.0f);
	s.Colors[ImGuiCol_HeaderHovered]      = ImVec4(0.043f, 0.07f, 0.137f, 1.0f);
	s.Colors[ImGuiCol_HeaderActive]       = ImVec4(0.043f, 0.07f, 0.137f, 1.0f);
	s.Colors[ImGuiCol_CheckMark]          = ImVec4(0.30f, 0.49f, 1.00f, 1.0f);
	s.Colors[ImGuiCol_SliderGrab]         = ImVec4(0.30f, 0.49f, 1.00f, 1.0f);
	s.Colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.30f, 0.49f, 1.00f, 1.0f);
	s.Colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.019f, 0.023f, 0.035f, 1.0f);
	s.Colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.031f, 0.035f, 0.058f, 1.0f);
	s.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.050f, 0.054f, 0.078f, 1.0f);
	s.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.07f, 0.074f, 0.098f, 1.0f);
	s.Colors[ImGuiCol_Separator]          = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
}

// ═══════════════════════════════════════════
//  辅助: 推入 neverlose 字体
// ═══════════════════════════════════════════
static void PushNLFont() { if (g_NLMainFont) PushFont(g_NLMainFont); }
static void PopNLFont()  { PopFont(); }

// ═══════════════════════════════════════════
//  Tab 内容渲染
// ═══════════════════════════════════════════

// ── ESP Tab (tab=0) ──
static void render_esp_tab() {
	PushNLFont();

	gui.group_box(ICON_FA_EYE " ESP", ImVec2(316, 290)); {
		gui.checkbox("Box",     &zeroflick::visuals::box);
		if (zeroflick::visuals::box)    { SameLine(260); gui.color_edit("##boxcol",    zeroflick::visuals::BOXcol); }

		gui.checkbox("Bone",    &zeroflick::visuals::bone);
		if (zeroflick::visuals::bone)   { SameLine(260); gui.color_edit("##bonecol",   zeroflick::visuals::Bonecol); }

		gui.checkbox("HP Bar",  &zeroflick::visuals::hp);
		if (zeroflick::visuals::hp)     { SameLine(260); gui.color_edit("##hpcol",     zeroflick::visuals::HPcol); }

		gui.checkbox("Name",    &zeroflick::visuals::name);
		if (zeroflick::visuals::name)   { SameLine(260); gui.color_edit("##namecol",   zeroflick::visuals::namecol); }

		gui.checkbox("Weapon",  &zeroflick::visuals::weapon);
		if (zeroflick::visuals::weapon) { SameLine(260); gui.color_edit("##weaponcol", zeroflick::visuals::weaponcol); }

		gui.checkbox("Snapline", &zeroflick::visuals::snapline);
		if (zeroflick::visuals::snapline) { SameLine(260); gui.color_edit("##snaplinecol", zeroflick::visuals::snaplinecol); }

		gui.checkbox("Distance", &zeroflick::visuals::distance);
	} gui.end_group_box();

	SameLine();

	gui.group_box(ICON_FA_FILTER " Filters", ImVec2(316 - GetStyle().ItemSpacing.x, 290)); {
		gui.checkbox("Visible only", &zeroflick::visuals::visible_only);
		gui.checkbox("Team check",   &zeroflick::visuals::team_check);
		Spacing();
		gui.slider_float("Max distance", &zeroflick::visuals::maxDist, 10.f, 500.f, "%.0f m");

		Spacing(); Spacing();
		gui.label_colored(gui.accent_color, ICON_FA_EYE " ESP renders through walls");
		gui.label_disabled("All features update in real-time");
	} gui.end_group_box();

	PopNLFont();
}

// ── Aimbot Tab (tab=1) — 带 subtab ──
static void render_aim_general() {
	gui.checkbox("Enable", &zeroflick::aim::aimbot);
	Spacing();

	gui.keybind("Aim Key", &zeroflick::aim::aimKey, &isBindingAimKey);
	Spacing();

	gui.slider_float("Smooth", &zeroflick::aim::smoothFactorValue, 0.1f, 10.f, "%.1f");

	Spacing();
	gui.label("Hitbox");
	SameLine(GetCursorPosX() + 80);
	gui.checkbox("Head", &zeroflick::aim::aimHead);
	SameLine(GetCursorPosX() + 170);
	gui.checkbox("Body", &zeroflick::aim::aimBody);
}

static void render_aim_rcs() {
	gui.checkbox("RCS Enable", &zeroflick::aim::autopunch);
	if (zeroflick::aim::autopunch) {
		Spacing();
		gui.slider_float("RCS Y", &zeroflick::aim::autopunchsenx, 0.1f, 5.f, "%.3f");
		gui.slider_float("RCS X", &zeroflick::aim::autopunchseny, 0.1f, 5.f, "%.3f");
	}
}

static void render_aim_misc() {
	gui.checkbox("Auto Fire", &zeroflick::aim::autoaim);
	Spacing();

	gui.keybind("Fire Key", &zeroflick::aim::triggerKey, &isBindingTriggerKey);
	Spacing();

	gui.checkbox("FOV Circle", &zeroflick::aim::fov);
	if (zeroflick::aim::fov)
		gui.slider_float("FOV Size", &zeroflick::aim::FOVSize, 0.01f, 360.f, "%.0f");

	Spacing();
	gui.checkbox("Inspect Enemy", &zeroflick::aim::inspectEn);
	if (zeroflick::aim::inspectEn)
		gui.slider_float("Inspect Size", &zeroflick::aim::inspectEnSize, 1.f, 50.f, "%.0f");
}

static void render_aim_tab() {
	PushNLFont();
	gui.group_box(ICON_FA_CROSSHAIRS " Aimbot", ImVec2(GetWindowWidth(), GetWindowHeight())); {

		switch (gui.m_rage_subtab) {
		case 0: render_aim_general(); break;
		case 1: render_aim_rcs();     break;
		case 2: render_aim_misc();    break;
		}

	} gui.end_group_box();
	PopNLFont();
}

// ── Trigger Tab (tab=2) ──
static void render_trigger_tab() {
	PushNLFont();

	gui.group_box(ICON_FA_MOUSE_POINTER " TriggerBot", ImVec2(316, 220)); {
		gui.checkbox("Auto Fire", &zeroflick::aim::autoaim);
		Spacing();
		gui.keybind("Fire Key", &zeroflick::aim::triggerKey, &isBindingTriggerKey);
		Spacing(); Spacing();
		gui.label_disabled("Hold aim key + fire key to activate");
	} gui.end_group_box();

	SameLine();

	gui.group_box(ICON_FA_RUNNING " Movement", ImVec2(316 - GetStyle().ItemSpacing.x, 220)); {
		gui.checkbox("Bunny Hop", &zeroflick::movement::bhop);
		Spacing();
		gui.checkbox("Auto Strafe", &zeroflick::misc::auto_strafe);
	} gui.end_group_box();

	PopNLFont();
}

// ── Visuals Tab (tab=3) ──
static void render_visuals_tab() {
	PushNLFont();

	gui.group_box(ICON_FA_PALETTE " Visuals", ImVec2(316, 200)); {
		gui.checkbox("Inspect Enemy", &zeroflick::aim::inspectEn);
		if (zeroflick::aim::inspectEn)
			gui.slider_float("Inspect Size", &zeroflick::aim::inspectEnSize, 1.f, 50.f, "%.0f");
	} gui.end_group_box();

	SameLine();

	gui.group_box(ICON_FA_EYE_SLASH " Anti-Flash", ImVec2(316 - GetStyle().ItemSpacing.x, 200)); {
		gui.checkbox("No Flash", &zeroflick::misc::no_flash);
		if (zeroflick::misc::no_flash)
			gui.slider_float("Flash Alpha", &zeroflick::misc::flash_alpha, 0.f, 1.f, "%.1f");
	} gui.end_group_box();

	PopNLFont();
}

// ── Config Tab (tab=4) ──
static void render_config_tab() {
	PushNLFont();

	gui.group_box(ICON_FA_SAVE " Config Manager", ImVec2(316, GetWindowHeight())); {
		if (gui.button("Refresh", ImVec2(100, 25))) {
			RefreshConfigList(); selectedConfig = -1;
			SetFeedback("Configs refreshed", ImVec4(0.3f, 0.49f, 1.f, 1.f));
		}
		SameLine();
		if (gui.button("Save", ImVec2(100, 25)) && selectedConfig >= 0 && selectedConfig < (int)configList.size())
			SaveCurrentConfig(configList[selectedConfig]);

		Spacing(); Spacing();
		gui.label("Configs:");
		Spacing();

		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
		PushID("cfg_scroll"); BeginChild("", ImVec2(GetContentRegionAvail().x, ImMin((int)configList.size() * 22.f, 120.f)), false, ImGuiWindowFlags_AlwaysUseWindowPadding);
		for (int i = 0; i < (int)configList.size(); i++)
			if (gui.selectable_item(configList[i].c_str(), selectedConfig == i))
				selectedConfig = i;
		EndChild(); PopID();
		PopStyleVar();

		if (selectedConfig >= 0 && selectedConfig < (int)configList.size()) {
			Spacing();
			if (gui.button("Load", ImVec2(90, 25)))
				LoadConfig(configList[selectedConfig]);
			SameLine();
			if (gui.button("Delete", ImVec2(90, 25))) {
				std::string fp = configList[selectedConfig] + ".ini";
				if (fs::exists(fp)) { fs::remove(fp); RefreshConfigList(); selectedConfig = -1; }
				SetFeedback("Deleted", ImVec4(1.f, 0.4f, 0.4f, 1.f));
			}
		}
	} gui.end_group_box();

	SameLine();

	gui.group_box(ICON_FA_PLUS " New Config", ImVec2(316 - GetStyle().ItemSpacing.x, GetWindowHeight())); {
		gui.label("Create a new config:");
		Spacing();
		gui.input_text("##newcfg", newConfigName, IM_ARRAYSIZE(newConfigName), "Config name...");
		Spacing();
		if (gui.button("Create", ImVec2(100, 25)) && strlen(newConfigName) > 0) {
			std::string nc = newConfigName;
			if (!nc.empty()) {
				SaveCurrentConfig(nc); RefreshConfigList();
				memset(newConfigName, 0, sizeof(newConfigName));
				auto it = std::find(configList.begin(), configList.end(), nc);
				if (it != configList.end()) selectedConfig = (int)std::distance(configList.begin(), it);
			}
		}

		Spacing(); Spacing(); Spacing();
		gui.label_disabled("Configs stored as .ini files");
		gui.label_colored(gui.accent_color, "ZEROBYTE " ICON_FA_HEART);
	} gui.end_group_box();

	PopNLFont();
}

// ═══════════════════════════════════════════
//  draw_Menu — neverlose 风格主菜单
// ═══════════════════════════════════════════
void draw_Menu() {

	// ── 按键绑定捕获 ──
	if (isBindingAimKey || isBindingTriggerKey) {
		for (int i = 1; i < 256; i++) {
			if (GetAsyncKeyState(i) & 0x8000) {
				if (isBindingAimKey)     zeroflick::aim::aimKey = i;
				if (isBindingTriggerKey) zeroflick::aim::triggerKey = i;
				isBindingAimKey = isBindingTriggerKey = false;
				break;
			}
		}
	}

	gui.m_anim = ImLerp(gui.m_anim, 1.f, 0.045f);

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	ImGui::Begin("ZEROBYTE", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground); {

		auto window = GetCurrentWindow();
		auto draw   = window->DrawList;
		auto pos    = window->Pos;
		auto size   = window->Size;

		SetWindowSize(ImVec2(690, 500));

		// ── 背景 ──
		draw->AddRectFilled(pos, pos + ImVec2(690, 500), ImColor(11, 12, 15), 0);

		// ── 标题 "ZEROBYTE" ──
		PushFont(g_NLTitleFont);
		auto title     = "ZEROBYTE";
		auto title_sz  = CalcTextSize(title);
		auto title_pos = ImVec2(170 / 2 - title_sz.x / 2 + 1, 20);

		draw->AddText(pos + title_pos, gui.accent_color.to_im_color(), title);
		draw->AddText(pos + title_pos - ImVec2(1, 0), GetColorU32(ImGuiCol_Text), title);
		PopFont();

		// ── 底部用户信息 ──
		draw->AddLine(pos + ImVec2(0, size.y - 50), pos + ImVec2(170, size.y - 50),
			GetColorU32(ImGuiCol_WindowBg, 0.5f));

		PushFont(g_NLMainFont);
		draw->AddText(pos + ImVec2(20, size.y - 42), gui.text.to_im_color(), "zerobyte");
		draw->AddText(pos + ImVec2(20, size.y - 25), gui.text_disabled.to_im_color(), "Till:");

		auto till_sz = CalcTextSize("Till: ");
		draw->AddText(pos + ImVec2(20 + till_sz.x, size.y - 25),
			gui.accent_color.to_im_color(), "Lifetime");
		PopFont();

		// ── 左侧边栏 tabs ──
		SetCursorPos(ImVec2(10, 70));
		PushID("tabs"); BeginChild("", ImVec2(150, size.y - 120));

		gui.group_title("Aimbot");
		if (gui.tab(ICON_FA_CROSSHAIRS, "ESP",     gui.m_tab == 0) && gui.m_tab != 0)
			gui.m_tab = 0, gui.m_anim = 0.f;
		if (gui.tab(ICON_FA_GHOST,     "Aimbot",   gui.m_tab == 1) && gui.m_tab != 1)
			gui.m_tab = 1, gui.m_anim = 0.f;
		if (gui.tab(ICON_FA_MOUSE,     "Trigger",  gui.m_tab == 2) && gui.m_tab != 2)
			gui.m_tab = 2, gui.m_anim = 0.f;

		Spacing(); Spacing(); Spacing();

		gui.group_title("Visuals");
		if (gui.tab(ICON_FA_PALETTE, "Visuals", gui.m_tab == 3) && gui.m_tab != 3)
			gui.m_tab = 3, gui.m_anim = 0.f;

		Spacing(); Spacing(); Spacing();

		gui.group_title("Settings");
		if (gui.tab(ICON_FA_COG, "Config", gui.m_tab == 4) && gui.m_tab != 4)
			gui.m_tab = 4, gui.m_anim = 0.f;

		EndChild(); PopID();

		// ── 右上 Save ──
		SetCursorPos(ImVec2(190, 20));
		{
			if (gui.button(ICON_FA_SAVE " Save", ImVec2(100, 25))) {
				SaveCurrentConfig("autosave");
				RefreshConfigList();
			}
		}

		// ── Sub-tabs (仅 Aimbot tab=1 显示) ──
		if (gui.m_tab == 1) {
			PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
			SetCursorPos(ImVec2(300, 20));
			PushID("subtabs"); BeginChild("", ImVec2(240, 25));

			GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(),
				gui.button_bg.to_im_color(), 4);
			GetWindowDrawList()->AddRect(GetWindowPos(), GetWindowPos() + GetWindowSize(),
				gui.border.to_im_color(), 4);

			for (int i = 0; i < (int)gui.rage_subtabs.size(); ++i) {
				if (gui.subtab(gui.rage_subtabs.at(i), gui.m_rage_subtab == i, (int)gui.rage_subtabs.size(),
					i == 0 ? ImDrawCornerFlags_Left :
					i == (int)gui.rage_subtabs.size() - 1 ? ImDrawCornerFlags_Right : 0)
					&& gui.m_rage_subtab != i)
					gui.m_rage_subtab = i, gui.m_anim = 0.f;

				if (i != (int)gui.rage_subtabs.size() - 1)
					SameLine();
			}

			EndChild(); PopID();
			PopStyleVar();
		}

		// ── 内容区 ──
		PushStyleVar(ImGuiStyleVar_Alpha, gui.m_anim);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

		float content_y = 81 - (5 * gui.m_anim);
		if (gui.m_tab != 1) content_y = 55;

		SetCursorPos(ImVec2(185, content_y));
		PushID("childs"); BeginChild("", ImVec2(size.x - 200, size.y - 70));

		switch (gui.m_tab) {
		case 0: render_esp_tab();     break;
		case 1: render_aim_tab();     break;
		case 2: render_trigger_tab(); break;
		case 3: render_visuals_tab(); break;
		case 4: render_config_tab();  break;
		}

		EndChild(); PopID();

		PopStyleVar(2);

	} ImGui::End();

	PopStyleVar();
}
