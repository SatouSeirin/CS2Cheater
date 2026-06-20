#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "../imgui_d11/imgui.h"
#include "../imgui_d11/imgui_internal.h"
#include "color_t.hpp"

using namespace ImGui;

extern ImFont* g_NLMainFont;

class c_gui {

public:

	float m_anim = 0.f;
	int m_tab = 0;
	int m_rage_subtab = 0;
	std::vector<const char*> rage_subtabs = { "General", "Anti-aim", "Misc" };

	color_t accent_color = { 0.3f, 0.49f, 1.f, 1.f };

	color_t text = { 1.f, 1.f, 1.f, 1.f };
	color_t text_disabled = { 0.51f, 0.52f, 0.56f, 1.f };

	color_t border = { 1.f, 1.f, 1.f, 0.03f };

	color_t frame_inactive = { 0.023f, 0.039f, 0.07f, 1.f };
	color_t frame_active = { 0.043f, 0.07f, 0.137f, 1.f };

	color_t button_bg = { 0.031f, 0.035f, 0.058f, 1.f };
	color_t button_hovered = { 0.050f, 0.054f, 0.078f, 1.f };
	color_t button_active = { 0.07f, 0.074f, 0.098f, 1.f };

	color_t group_box_bg = { 0.019f, 0.035f, 0.062f, 1.f };

	// ── 内联工具函数 ──
	void render_circle_for_horizontal_bar(ImVec2 pos, ImColor color, float alpha) {
		auto draw = GetWindowDrawList();
		draw->AddCircleFilled(pos, 6, ImColor(color.Value.x, color.Value.y, color.Value.z, alpha * GetStyle().Alpha));
	}

	inline void group_title(const char* name) {
		SetCursorPosX(GetCursorPosX() + 10);
		PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.5f));
		Text(name);
		PopStyleColor();
	}

	// ── 容器控件: tab / subtab / group_box ──
	bool tab(const char* icon, const char* label, bool selected) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);

		auto icon_size = CalcTextSize(icon);
		auto label_size = CalcTextSize(label, 0, 1);

		auto pos = window->DC.CursorPos;
		auto draw = window->DrawList;

		ImRect bb(pos, pos + ImVec2(GetWindowWidth(), 30));
		ItemAdd(bb, id);
		ItemSize(bb, GetStyle().FramePadding.y);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		static std::unordered_map<ImGuiID, float> values;
		auto value = values.find(id);
		if (value == values.end()) {
			values.insert({ id, 0.f });
			value = values.find(id);
		}

		value->second = ImLerp(value->second, (selected ? 1.f : 0.f), 0.05f);

		draw->AddRectFilled(bb.Min, bb.Max, frame_active.to_im_color(0.5f * value->second), 5);

		draw->AddText(ImVec2(bb.Min.x + 10, bb.GetCenter().y - label_size.y / 2), accent_color.to_im_color(), icon);
		draw->AddText(ImVec2(bb.Min.x + 35, bb.GetCenter().y - label_size.y / 2), GetColorU32(ImGuiCol_Text), label);

		return pressed;
	}

	bool subtab(const char* label, bool selected, int size, ImDrawCornerFlags flags) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);

		auto label_size = CalcTextSize(label, 0, 1);

		auto pos = window->DC.CursorPos;
		auto draw = window->DrawList;

		ImRect bb(pos, pos + ImVec2(GetWindowWidth() / size, GetWindowHeight()));
		ItemAdd(bb, id);
		ItemSize(bb, GetStyle().FramePadding.y);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		static std::unordered_map<ImGuiID, float> values;
		auto value = values.find(id);
		if (value == values.end()) {
			values.insert({ id, 0.f });
			value = values.find(id);
		}

		value->second = ImLerp(value->second, (selected ? 1.f : 0.f), 0.05f);

		draw->AddRectFilled(bb.Min, bb.Max, frame_active.to_im_color(0.8f * value->second), 4, flags);

		draw->AddText(bb.GetCenter() - label_size / 2, selected ? text.to_im_color() : text_disabled.to_im_color(), label);

		return pressed;
	}

	void group_box(const char* name, ImVec2 size_arg) {
		auto window = GetCurrentWindow();
		auto pos = window->DC.CursorPos;

		// PushID 保证嵌套 group_box 的 ID 隔离 + 空字符串 BeginChild 不产生任何文字
		PushID(name);
		BeginChild("", size_arg, false, ImGuiWindowFlags_NoScrollbar);

		GetWindowDrawList()->AddRectFilled(pos + ImVec2(0, 20), pos + size_arg, group_box_bg.to_im_color(), 6);
		GetWindowDrawList()->AddRect(pos + ImVec2(0, 20), pos + size_arg, border.to_im_color(), 6);

		PushFont(g_NLMainFont);
		GetWindowDrawList()->AddText(pos + ImVec2(12, 0), GetColorU32(ImGuiCol_Text, 0.5f), name);
		PopFont();

		SetCursorPos(ImVec2(12, 21));
		PushStyleVar(ImGuiStyleVar_WindowPadding, { 0, 10 });
		PushID("inner");
		BeginChild("", { size_arg.x - 24, size_arg.y - 21 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysUseWindowPadding);

		BeginGroup();
		PushStyleVar(ImGuiStyleVar_ItemSpacing, { 8, 10 });
		PushStyleVar(ImGuiStyleVar_Alpha, m_anim);
	}

	void end_group_box() {
		PopStyleVar(3);
		EndGroup();
		EndChild();
		PopID();     // "inner" PushID
		EndChild();
		PopID();     // name PushID
	}

	// ═══════════════════════════════════════════
	//  自定义控件 — neverlose 风格
	// ═══════════════════════════════════════════

	// ── Checkbox (toggle switch) ──
	bool checkbox(const char* label, bool* v) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;
		auto label_size = CalcTextSize(label);

		float toggle_w = 24.f, toggle_h = 14.f;
		// 只覆盖 toggle + 间距 + label，不再占满整行，避免吃掉同行 color_edit 的点击
		float bb_w = toggle_w + 8 + label_size.x;
		ImRect bb(pos, pos + ImVec2(bb_w, ImMax(toggle_h, label_size.y) + 4));
		ItemAdd(bb, id);
		ItemSize(bb);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);
		if (pressed) *v = !*v;

		static std::unordered_map<ImGuiID, float> anims;
		auto anim = anims.find(id);
		if (anim == anims.end()) { anims.insert({ id, *v ? 1.f : 0.f }); anim = anims.find(id); }
		anim->second = ImLerp(anim->second, *v ? 1.f : 0.f, 0.12f);

		float knob_x = pos.x + 2 + (toggle_w - 14) * anim->second;
		ImVec2 toggle_pos(pos.x, pos.y + (bb.GetHeight() - toggle_h) * 0.5f);

		// 背景
		draw->AddRectFilled(toggle_pos, toggle_pos + ImVec2(toggle_w, toggle_h),
			frame_inactive.to_im_color(), toggle_h * 0.5f);

		// 激活填充
		draw->AddRectFilled(toggle_pos, toggle_pos + ImVec2(toggle_w * anim->second, toggle_h),
			accent_color.to_im_color(0.8f), toggle_h * 0.5f);

		// 旋钮
		draw->AddCircleFilled(ImVec2(knob_x + 7, toggle_pos.y + toggle_h * 0.5f), 6,
			text.to_im_color(0.9f));

		// 标签
		draw->AddText(ImVec2(pos.x + toggle_w + 8, pos.y + (bb.GetHeight() - label_size.y) * 0.5f),
			GetColorU32(ImGuiCol_Text), label);

		return pressed;
	}

	// ── SliderFloat ──
	bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f") {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;

		float bar_h = 6.f;
		float spacing = 6.f;
		char val_buf[32];
		ImFormatString(val_buf, IM_ARRAYSIZE(val_buf), format, *v);
		auto val_size = CalcTextSize(val_buf);

		auto label_size = CalcTextSize(label);

		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, label_size.y + bar_h + spacing + 6));
		ItemAdd(bb, id);
		ItemSize(bb);

		// 标签 + 值
		draw->AddText(ImVec2(pos.x, pos.y), GetColorU32(ImGuiCol_Text), label);
		draw->AddText(ImVec2(pos.x + GetContentRegionAvail().x - val_size.x, pos.y),
			accent_color.to_im_color(), val_buf);

		// Slider bar
		ImVec2 bar_pos(pos.x, pos.y + label_size.y + spacing);
		ImRect bar_bb(bar_pos, bar_pos + ImVec2(GetContentRegionAvail().x, bar_h));

		float t = (*v - v_min) / (v_max - v_min);
		t = ImClamp(t, 0.f, 1.f);

		draw->AddRectFilled(bar_bb.Min, bar_bb.Max, frame_inactive.to_im_color(), bar_h * 0.5f);
		draw->AddRectFilled(bar_bb.Min, ImVec2(bar_bb.Min.x + bar_bb.GetWidth() * t, bar_bb.Max.y),
			accent_color.to_im_color(), bar_h * 0.5f);

		// 交互
		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);
		if (held || pressed) {
			float mx = ImClamp(GetIO().MousePos.x, bar_bb.Min.x, bar_bb.Max.x);
			*v = v_min + (v_max - v_min) * ((mx - bar_bb.Min.x) / bar_bb.GetWidth());
		}

		// 拖拽圆点
		float grab_x = bar_bb.Min.x + bar_bb.GetWidth() * t;
		draw->AddCircleFilled(ImVec2(grab_x, bar_bb.GetCenter().y),
			held ? 7.f : 5.f, accent_color.to_im_color(held ? 1.f : 0.8f));

		return held || pressed;
	}

	// ── 普通按钮 ──
	bool button(const char* label, const ImVec2& size_arg = ImVec2(0, 0)) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;

		auto label_size = CalcTextSize(label);
		ImVec2 size = size_arg;
		if (size.x <= 0) size.x = label_size.x + 20;
		if (size.y <= 0) size.y = label_size.y + 8;

		ImRect bb(pos, pos + size);
		ItemAdd(bb, id);
		ItemSize(bb);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		auto col = held ? button_active : hovered ? button_hovered : button_bg;
		draw->AddRectFilled(bb.Min, bb.Max, col.to_im_color(), 4);
		draw->AddText(bb.GetCenter() - label_size * 0.5f, GetColorU32(ImGuiCol_Text), label);

		return pressed;
	}

	// ── 按键绑定按钮 ──
	bool keybind(const char* label, int* key, bool* is_binding) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;

		const char* key_name = *is_binding ? "[ ... ]" : GetKeyName(*key);
		char display[64];
		ImFormatString(display, IM_ARRAYSIZE(display), "%s", key_name);

		auto label_size = CalcTextSize(display);
		float w = ImMax(GetContentRegionAvail().x, label_size.x + 20);
		float h = label_size.y + 12;

		ImRect bb(pos, pos + ImVec2(w, h));
		ItemAdd(bb, id);
		ItemSize(bb);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);
		if (pressed) *is_binding = !*is_binding;

		auto col = *is_binding ? accent_color :
			hovered ? button_hovered : button_bg;
		draw->AddRectFilled(bb.Min, bb.Max, col.to_im_color(), 4);
		draw->AddText(bb.GetCenter() - label_size * 0.5f, GetColorU32(ImGuiCol_Text), display);

		return pressed;
	}

	static const char* GetKeyName(int key) {
		switch (key) {
		case VK_LBUTTON:  return "Mouse1";
		case VK_RBUTTON:  return "Mouse2";
		case VK_MBUTTON:  return "Mouse3";
		case VK_XBUTTON1: return "Mouse4";
		case VK_XBUTTON2: return "Mouse5";
		case VK_BACK:     return "Back";
		case VK_TAB:      return "Tab";
		case VK_RETURN:   return "Enter";
		case VK_SHIFT:    return "Shift";
		case VK_CONTROL:  return "Ctrl";
		case VK_MENU:     return "Alt";
		case VK_SPACE:    return "Space";
		case VK_ESCAPE:   return "Esc";
		case VK_LEFT:     return "Left";
		case VK_UP:       return "Up";
		case VK_RIGHT:    return "Right";
		case VK_DOWN:     return "Down";
		default:
			if (key >= 'A' && key <= 'Z') { static char c[2]; c[0] = (char)key, c[1] = 0; return c; }
			if (key >= '0' && key <= '9') { static char c[2]; c[0] = (char)key, c[1] = 0; return c; }
			return "...";
		}
	}

	// ── 颜色编辑器 ──
	bool color_edit(const char* label, float col[4], bool alpha = true) {
		auto window = GetCurrentWindow();
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;

		float preview_sz = 14.f;
		ImRect bb(pos, pos + ImVec2(preview_sz, preview_sz));

		// 自定义颜色预览框
		ImU32 col32 = ImColor(col[0], col[1], col[2], col[3]);
		draw->AddRectFilled(bb.Min, bb.Max, col32, 3);
		draw->AddRect(bb.Min, bb.Max, border.to_im_color(), 3);

		// 透明按钮覆盖在上面用于交互 — 用标准 ImGui popup，不会干扰父窗口布局
		SetCursorScreenPos(bb.Min);
		InvisibleButton(label, ImVec2(preview_sz, preview_sz));

		bool clicked = IsItemClicked();
		if (clicked)
			ImGui::OpenPopup(label);

		if (ImGui::BeginPopup(label)) {
			ImGui::ColorPicker4("##picker", col,
				ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
				(alpha ? ImGuiColorEditFlags_AlphaBar : ImGuiColorEditFlags_NoAlpha) |
				ImGuiColorEditFlags_Float);
			ImGui::EndPopup();
		}

		return clicked;
	}

	// ── 可选项 (列表) ──
	bool selectable_item(const char* label, bool selected) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;
		auto label_size = CalcTextSize(label);

		float h = label_size.y + 6;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, h));
		ItemAdd(bb, id);
		ItemSize(bb);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		static std::unordered_map<ImGuiID, float> anims;
		auto anim = anims.find(id);
		if (anim == anims.end()) { anims.insert({ id, selected ? 1.f : 0.f }); anim = anims.find(id); }
		anim->second = ImLerp(anim->second, selected ? 1.f : 0.f, 0.1f);

		draw->AddRectFilled(bb.Min, bb.Max,
			selected ? accent_color.to_im_color(0.15f) :
			hovered ? button_hovered.to_im_color(0.5f) :
			ImColor(0, 0, 0, 0), 4);

		draw->AddText(ImVec2(pos.x + 8, bb.GetCenter().y - label_size.y * 0.5f),
			selected ? (ImU32)accent_color.to_im_color() : GetColorU32(ImGuiCol_Text), label);

		return pressed;
	}

	// ── 文本输入 ──
	bool input_text(const char* label, char* buf, size_t buf_size, const char* hint = nullptr) {
		auto window = GetCurrentWindow();
		auto id = window->GetID(label);
		auto draw = window->DrawList;
		auto pos = window->DC.CursorPos;

		float h = 28.f;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, h));
		ItemAdd(bb, id);
		ItemSize(bb);

		bool hovered, held;
		bool pressed = ButtonBehavior(bb, id, &hovered, &held);

		// 背景
		draw->AddRectFilled(bb.Min, bb.Max, frame_inactive.to_im_color(), 4);
		draw->AddRect(bb.Min, bb.Max, border.to_im_color(), 4);

		if (pressed)
			ImGui::SetKeyboardFocusHere();

		bool active = ImGui::IsItemActive();
		if (active) {
			ImGuiIO& io = GetIO();
			for (int n = 0; n < io.InputQueueCharacters.Size && strlen(buf) < buf_size - 1; n++) {
				if (io.InputQueueCharacters[n] > 0 && io.InputQueueCharacters[n] < 0x10000) {
					unsigned int c = (unsigned int)io.InputQueueCharacters[n];
					char utf8[5] = {};
					int len = 0;
					if (c < 0x80) { utf8[0] = (char)c; len = 1; }
					else if (c < 0x800) { utf8[0] = (char)(0xc0 + (c >> 6)); utf8[1] = (char)(0x80 + (c & 0x3f)); len = 2; }
					else { utf8[0] = (char)(0xe0 + (c >> 12)); utf8[1] = (char)(0x80 + ((c >> 6) & 0x3f)); utf8[2] = (char)(0x80 + (c & 0x3f)); len = 3; }
					strcat_s(buf, buf_size, utf8);
				}
			}
			if (IsKeyPressed(ImGuiKey_Backspace) && strlen(buf) > 0)
				buf[strlen(buf) - 1] = 0;
		}

		const char* display = (strlen(buf) > 0) ? buf : (hint ? hint : "");
		auto text_size = CalcTextSize(display);
		ImU32 text_col = strlen(buf) > 0 ? GetColorU32(ImGuiCol_Text) : GetColorU32(ImGuiCol_TextDisabled);

		draw->AddText(ImVec2(pos.x + 8, bb.GetCenter().y - text_size.y * 0.5f),
			text_col, display);

		// 闪烁光标
		if (active && (int)(GetTime() * 2) % 2) {
			float cursor_x = pos.x + 8 + ImMax(text_size.x, 0.f);
			draw->AddLine(ImVec2(cursor_x, pos.y + 6), ImVec2(cursor_x, pos.y + h - 6),
				accent_color.to_im_color());
		}

		return active;
	}

	// ── 纯文本 ──
	void label(const char* fmt) {
		auto pos = GetCurrentWindow()->DC.CursorPos;
		auto size = CalcTextSize(fmt);
		GetWindowDrawList()->AddText(pos, GetColorU32(ImGuiCol_Text), fmt);
		ItemSize(ImRect(pos, pos + size));
	}

	void label_colored(const color_t& c, const char* fmt) {
		auto pos = GetCurrentWindow()->DC.CursorPos;
		auto size = CalcTextSize(fmt);
		GetWindowDrawList()->AddText(pos, c.to_im_color(), fmt);
		ItemSize(ImRect(pos, pos + size));
	}

	void label_disabled(const char* fmt) {
		label_colored(text_disabled, fmt);
	}
};

inline c_gui gui;
