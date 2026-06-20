#pragma once
#include "../imgui_d11/imgui.h"
using namespace ImGui;

struct color_t {

public:
	float r, g, b, a;

	color_t() : r(1.f), g(1.f), b(1.f), a(1.f) {}
	color_t(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}

	inline ImColor to_im_color(float alpha = 1.f, bool use_gl_alpha = true) const {
		return ImColor(r, g, b, (a * (use_gl_alpha ? GetStyle().Alpha : 1.f)) * alpha);
	}

	inline ImVec4 to_vec4(float alpha = 1.f, bool use_gl_alpha = true) const {
		return ImVec4(r, g, b, (a * (use_gl_alpha ? GetStyle().Alpha : 1.f)) * alpha);
	}
};
