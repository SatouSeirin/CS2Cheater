#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinUser.h>
#include "../imgui_d11/imgui.h"
#include <string>
namespace zeroflick {
	namespace visuals {
		inline bool box = false;
		inline float BOXcol[4] = { 0.0f, 1.0f, 0.82f, 1.0f }; // 初始化默认值（RGBA）

	    inline bool bone = false;
		inline float Bonecol[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // 红色默认值

        inline bool name = false;
        inline float namecol[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // 红色默认值

	    inline bool hp = false;
		inline float HPcol[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; // ??????????

        inline bool weapon = false;
        inline float weaponcol[4] = { 1.0f, 1.0f, 0.0f, 1.0f }; // 黄色
	}

  namespace aim {
       inline bool aimbot = true;
       inline bool autoaim = false;
       inline bool autopunch = false;
        inline bool inspectEn = false;
        inline bool fov = false;
      inline float FOVSize = 100.0f;
	  inline float autopunchsenx = 1.000f; // 自动打枪灵敏度系数
	  inline float autopunchseny = 1.000f; // 自动打枪灵敏度系数
      inline float smoothFactorValue = 4.0f;
       inline float inspectEnSize = 10.0f;

       inline int aimKey = VK_MENU; // 默认ALT键 (自瞄热键)
       inline int triggerKey = VK_SHIFT; // 默认SHIFT键 (自动扳机热键)
       inline int aimPart = 0; // 0=头部, 1=胸部, 2=腹部

        inline bool aimHead = true;   // 默认瞄准头部
       inline bool aimBody = false;  // 瞄准胸部
       inline bool aimDick = false;  // 瞄准盆骨
   }

	namespace movement {
	inline bool bhop = false;
    }
}

namespace gui {
    void Initialize();  // 声明初始化函数
}


// 反馈结构体
struct Feedback {
    std::string message;
    float timer = 0.0f;
    ImVec4 color = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
};

extern Feedback g_feedback;  // 声明全局变量

void draw_Menu();

// 反馈系统
void ShowFeedback();
void SetFeedback(const std::string& message,
    const ImVec4& color = ImVec4(0.26f, 0.59f, 0.98f, 1.0f),
    float duration = 2.0f);// 声明时指定默认参数
void ShowFeedback();
// 配置管理
void SaveCurrentConfig(const std::string& filename);
void LoadConfig(const std::string& filename);
void RefreshConfigList();
