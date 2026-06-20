
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinUser.h>
#include <fstream>
#include <sstream>
#include "../imgui_d11/imgui.h"
#include "./gui.h"
#include "../imgui_d11/imgui_internal.h"
#include <string>
#include <algorithm>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;


inline ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x + b.x, a.y + b.y);
}
inline ImVec2 operator-(const ImVec2& a, const ImVec2& b) {
    return ImVec2(a.x - b.x, a.y - b.y);
}

// 窗口大小
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

// 全局变量存储窗口位置
static ImVec2 windowPos = ImVec2(100, 100);
static bool isDragging = false;
static ImVec2 dragOffset;


// 新增：按键绑定相关
static bool isBindingAimKey = false;
static bool isBindingTriggerKey = false;
static const char* GetKeyName(int key) {
    static std::string keyName; // 静态存储，避免返回临时字符串

    switch (key) {
    case VK_LBUTTON: return "Mouse Left";
    case VK_RBUTTON: return "Mouse Right";
    case VK_MBUTTON: return "Mouse Middle";
    case VK_XBUTTON1: return "Mouse X1";
    case VK_XBUTTON2: return "Mouse X2";
    case VK_BACK: return "Backspace";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "Caps Lock";
    case VK_ESCAPE: return "Escape";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left Arrow";
    case VK_UP: return "Up Arrow";
    case VK_RIGHT: return "Right Arrow";
    case VK_DOWN: return "Down Arrow";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_NUMPAD0: return "Numpad 0";
        // ... 其他按键可以继续补充
    default:
        if (key >= 'A' && key <= 'Z') {
            keyName = static_cast<char>(key);
            return keyName.c_str();
        }
        if (key >= '0' && key <= '9') {
            keyName = static_cast<char>(key);
            return keyName.c_str();
        }
        return "Unknown";
    }
}

// 定义菜单状态
enum MenuTab {
    TAB_ESP,
    TAB_AIM,
    TAB_MISC,
    TAB_SETTINGS,  // 新增设置标签
    TAB_COUNT
};
static MenuTab currentTab = TAB_ESP;



// 辅助函数：分割字符串
static std::vector<std::string> SplitString(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// 辅助函数：去除字符串两端的空白字符
static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// 全局变量存储配置列表
static std::vector<std::string> configList;
static int selectedConfig = -1;
static char newConfigName[64] = "";
// 反馈消息相关
static char feedbackMessage[256] = "";
static float feedbackTimer = 0.0f;
static ImVec4 feedbackColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // 默认绿色
// 显示反馈消息

Feedback g_feedback;

// gui.cpp
void ShowFeedback() {
    if (g_feedback.timer <= 0.0f) return;

    // 计算淡出透明度
    float alpha = ImMin(g_feedback.timer / 0.5f, 1.0f); // 最后0.5秒淡出

    // 设置窗口样式（与主界面一致）
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.7f * alpha));

    // 居中显示（占屏幕宽度50%）
    ImVec2 windowSize = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 0);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
        ImGui::GetIO().DisplaySize.y * 0.4f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(windowSize);

    if (ImGui::Begin("##Feedback", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs))
    {
        // 文字居中
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // 使用主字体
        ImVec2 textSize = ImGui::CalcTextSize(g_feedback.message.c_str());
        ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);

        // 文字样式（带透明度）
        ImVec4 textColor = g_feedback.color;
        textColor.w = alpha;
        ImGui::TextColored(textColor, "%s", g_feedback.message.c_str());

        ImGui::PopFont();
    }
    ImGui::End();

    // 恢复样式
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    // 更新计时器
    g_feedback.timer -= ImGui::GetIO().DeltaTime;
}

void SetFeedback(const std::string& message, const ImVec4& color, float duration) {
    g_feedback.message = message;
    g_feedback.color = color;
    g_feedback.timer = duration;
}

void SaveCurrentConfig(const std::string& filename) {
    std::ofstream file(filename + ".ini");
    if (file.is_open()) {
        // 保存ESP设置
        file << "[ESP]\n";
        file << "box=" << zeroflick::visuals::box << "\n";
        file << "bone=" << zeroflick::visuals::bone << "\n";
        file << "hp=" << zeroflick::visuals::hp << "\n";
        file << "name=" << zeroflick::visuals::name << "\n";
        file << "weapon=" << zeroflick::visuals::weapon << "\n";

        // 保存颜色设置
        file << "box_color=" << zeroflick::visuals::BOXcol[0] << ","
            << zeroflick::visuals::BOXcol[1] << ","
            << zeroflick::visuals::BOXcol[2] << ","
            << zeroflick::visuals::BOXcol[3] << "\n";

        file << "bone_color=" << zeroflick::visuals::Bonecol[0] << ","
            << zeroflick::visuals::Bonecol[1] << ","
            << zeroflick::visuals::Bonecol[2] << ","
            << zeroflick::visuals::Bonecol[3] << "\n";

        // 保存AIM设置
        file << "[AIM]\n";
        file << "aimbot=" << zeroflick::aim::aimbot << "\n";
        file << "aim_key=" << zeroflick::aim::aimKey << "\n";
        file << "smoothFactorValue=" << zeroflick::aim::smoothFactorValue << "\n";
        file << "autoaim=" << zeroflick::aim::autoaim << "\n";
        file << "triggerKey=" << zeroflick::aim::triggerKey << "\n";

        file.close();
        SetFeedback(std::string("配置已保存: " + filename).c_str(), ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
    else {
        SetFeedback(std::string("保存失败: " + filename).c_str(), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}

void LoadConfig(const std::string& filename) {
    std::ifstream file(filename + ".ini");
    if (file.is_open()) {
        std::string line;
        std::string section;

        while (std::getline(file, line)) {
            line = Trim(line);
            if (line.empty()) continue;

            if (line[0] == '[') {
                section = line.substr(1, line.find(']') - 1);
            }
            else {
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = Trim(line.substr(0, eq_pos));
                    std::string value = Trim(line.substr(eq_pos + 1));

                    if (section == "ESP") {
                        if (key == "box") zeroflick::visuals::box = (value == "1");
                        else if (key == "bone") zeroflick::visuals::bone = (value == "1");
                        else if (key == "hp") zeroflick::visuals::hp = (value == "1");
                        else if (key == "name") zeroflick::visuals::name = (value == "1");
                        else if (key == "weapon") zeroflick::visuals::weapon = (value == "1");

                        // 处理颜色
                        else if (key == "box_color") {
                            auto colors = SplitString(value, ',');
                            if (colors.size() == 4) {
                                zeroflick::visuals::BOXcol[0] = std::stof(colors[0]);
                                zeroflick::visuals::BOXcol[1] = std::stof(colors[1]);
                                zeroflick::visuals::BOXcol[2] = std::stof(colors[2]);
                                zeroflick::visuals::BOXcol[3] = std::stof(colors[3]);
                            }
                        }
                        else if (key == "bone_color") {
                            auto colors = SplitString(value, ',');
                            if (colors.size() == 4) {
                                zeroflick::visuals::Bonecol[0] = std::stof(colors[0]);
                                zeroflick::visuals::Bonecol[1] = std::stof(colors[1]);
                                zeroflick::visuals::Bonecol[2] = std::stof(colors[2]);
                                zeroflick::visuals::Bonecol[3] = std::stof(colors[3]);
                            }
                        }
                    }
                    else if (section == "AIM") {
                        if (key == "aimbot") zeroflick::aim::aimbot = (value == "1");
                        else if (key == "aim_key") zeroflick::aim::aimKey = std::stoi(value);
                        else if (key == "smoothFactorValue") zeroflick::aim::smoothFactorValue = std::stof(value);
                        else if (key == "autoaim") zeroflick::aim::autoaim = (value == "1");
                        else if (key == "triggerKey") zeroflick::aim::triggerKey = std::stoi(value);
                    }
                }
            }
        }
        file.close();
        SetFeedback(std::string("配置已加载: " + filename).c_str(), ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
    }
    else {
        SetFeedback(std::string("加载失败: " + filename).c_str(), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
}


void RefreshConfigList() {
    configList.clear();

    // 获取当前目录下所有.ini文件
    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.path().extension() == ".ini") {
            configList.push_back(entry.path().stem().string());
        }
    }

    // 按字母排序
    std::sort(configList.begin(), configList.end());
}

void gui::Initialize() {
    // 获取ImGui样式引用
    ImGuiStyle& style = ImGui::GetStyle();
    // 全局样式设置
    style.WindowRounding = 12.0f;         // 窗口圆角
    style.FrameRounding = 4.0f;          // 控件圆角
    style.GrabRounding = 4.0f;           // 滑块圆角
    style.ScrollbarRounding = 9.0f;      // 滚动条圆角
    style.WindowBorderSize = 0.0f;       // 无窗口边框
    style.FrameBorderSize = 0.0f;        // 无控件边框

    // 颜色设置
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f); // 对勾颜色
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);   // 控件背景
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    // 按钮样式
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

    // 注意：字体已在 DllMain.cpp 的 my_present 中加载，这里不再重复加载

    // 初始化时刷新配置列表
    RefreshConfigList();
}
// 美化后的Checkbox
bool CustomCheckbox(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    const float square_sz = ImGui::GetFrameHeight();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f), label_size.y + style.FramePadding.y * 2.0f));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
        *v = !(*v);

    // 渲染
    const ImU32 col_bg = hovered ? ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBgHovered) : ImGui::GetColorU32(ImGuiCol_FrameBg);
    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Min + ImVec2(square_sz, square_sz), col_bg, style.FrameRounding);

    if (*v) {
        const float pad = ImMax(1.0f, (float)(int)(square_sz / 6.0f));
        ImGui::RenderCheckMark(window->DrawList, total_bb.Min + ImVec2(pad, pad), ImGui::GetColorU32(ImGuiCol_CheckMark), square_sz - pad * 2.0f);
    }

    if (label_size.x > 0.0f) {
        ImGui::RenderText(ImVec2(total_bb.Min.x + square_sz + style.ItemInnerSpacing.x, total_bb.Min.y + style.FramePadding.y), label);
    }

    return pressed;
}

// 美化后的ColorEdit4
bool CustomColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const float square_sz = ImGui::GetFrameHeight();
    const float w_full = ImGui::CalcItemWidth();
    const float w_button = (flags & ImGuiColorEditFlags_NoSmallPreview) ? 0.0f : (square_sz + style.ItemInnerSpacing.x);
    const float w_inputs = w_full - w_button;

    const char* label_display_end = ImGui::FindRenderedTextEnd(label);

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, pos + ImVec2(w_full, square_sz));

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    bool value_changed = false;

    // 渲染颜色预览
    if (!(flags & ImGuiColorEditFlags_NoSmallPreview)) {
        ImU32 col_u32 = ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
        window->DrawList->AddRectFilled(total_bb.Min, total_bb.Min + ImVec2(square_sz, square_sz), col_u32, style.FrameRounding);
    }

    // 渲染文本标签
    if (label != label_display_end) {
        ImGui::RenderText(ImVec2(total_bb.Min.x + square_sz + style.ItemInnerSpacing.x, total_bb.Min.y + style.FramePadding.y), label);
    }

    // 处理交互
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) {
        ImGui::OpenPopup("color_picker");
        ImGui::SetNextWindowPos(ImGui::GetItemRectMax() + ImVec2(0, style.ItemSpacing.y));
    }

    // 颜色选择器弹出窗口
    if (ImGui::BeginPopup("color_picker")) {
        if (flags & ImGuiColorEditFlags_PickerHueWheel) {
            value_changed = ImGui::ColorPicker4("##picker", col, flags | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
        }
        else {
            value_changed = ImGui::ColorPicker4("##picker", col, flags);
        }
        ImGui::EndPopup();
    }

    return value_changed;
}




void draw_Menu() {

    // 在消息循环或主循环中添加按键绑定处理
    if (isBindingAimKey || isBindingTriggerKey) {
        // 优先检测鼠标按键
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (isBindingAimKey) zeroflick::aim::aimKey = VK_LBUTTON;
            if (isBindingTriggerKey) zeroflick::aim::triggerKey = VK_LBUTTON;
            isBindingAimKey = isBindingTriggerKey = false;
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (isBindingAimKey) zeroflick::aim::aimKey = VK_RBUTTON;
            if (isBindingTriggerKey) zeroflick::aim::triggerKey = VK_RBUTTON;
            isBindingAimKey = isBindingTriggerKey = false;
        }
        // 检测键盘按键
        else {
            for (int i = 0; i < 256; i++) {
                if (i == VK_LBUTTON || i == VK_RBUTTON) continue;
                if (GetAsyncKeyState(i) & 0x8000) {
                    if (isBindingAimKey) zeroflick::aim::aimKey = i;
                    if (isBindingTriggerKey) zeroflick::aim::triggerKey = i;
                    isBindingAimKey = isBindingTriggerKey = false;
                    break;
                }
            }
        }
    }
    // 设置窗口位置和大小
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH, WINDOW_HEIGHT), ImGuiCond_Always);

    // 窗口样式设置 - 添加圆角
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f); // 圆角大小
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("MONSAL", nullptr, ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);
    {

        // 拖动检测 - 在任何位置都可以拖动
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
            windowPos.x += ImGui::GetIO().MouseDelta.x;
            windowPos.y += ImGui::GetIO().MouseDelta.y;
            ImGui::SetWindowPos(windowPos);
        }

        // 开始3:7布局
        ImGui::Columns(2, "MainColumns", false);
        ImGui::SetColumnWidth(0, WINDOW_WIDTH * 0.3f); // 30%宽度给左侧

        // 左侧面板 - 整体背景
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::BeginChild("LeftPanel", ImVec2(0, 0), true);
        {
            // Logo区域 - 顶部20%高度
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::BeginChild("LogoArea", ImVec2(0, WINDOW_HEIGHT * 0.2f), true);
            {
                // 这里放置Logo - 示例使用文字代替
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // 使用大号字体
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("MONSAL").x) * 0.5f);
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "MONSAL");
                ImGui::PopFont();

                // 副标题
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("CS2 Cheat").x) * 0.5f);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CS2 Cheat");
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // 按钮区域 - 剩余80%高度
            ImGui::BeginChild("ButtonArea", ImVec2(0, 0), true);
            {
                // 按钮样式美化
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f); // 更大的圆角
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 12)); // 内边距

                // 计算居中位置
                float buttonWidth = ImGui::GetContentRegionAvail().x * 0.8f;
                float buttonHeight = 45.0f;
                float buttonSpacing = 15.0f;

                // 按钮颜色设置
                auto SetButtonColors = [](const ImVec4& baseColor) {
                    ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
                        baseColor.x * 1.2f,
                        baseColor.y * 1.2f,
                        baseColor.z * 1.2f,
                        baseColor.w
                    ));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(
                        baseColor.x * 0.8f,
                        baseColor.y * 0.8f,
                        baseColor.z * 0.8f,
                        baseColor.w
                    ));
                    };
            

                // 添加顶部间距
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);

                // ESP按钮
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
                SetButtonColors(currentTab == TAB_ESP ? ImVec4(0.26f, 0.59f, 0.98f, 0.8f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
                if (ImGui::Button("透视", ImVec2(buttonWidth, buttonHeight))) currentTab = TAB_ESP;
                ImGui::PopStyleColor(3);

                // 按钮间距
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonSpacing);

                // AIM按钮
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
                SetButtonColors(currentTab == TAB_AIM ? ImVec4(0.26f, 0.59f, 0.98f, 0.8f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
                if (ImGui::Button("自瞄", ImVec2(buttonWidth, buttonHeight))) currentTab = TAB_AIM;
                ImGui::PopStyleColor(3);

                // 按钮间距
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonSpacing);

                // MOVEMENT按钮
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
                SetButtonColors(currentTab == TAB_MISC ? ImVec4(0.26f, 0.59f, 0.98f, 0.8f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
                if (ImGui::Button("运动", ImVec2(buttonWidth, buttonHeight))) currentTab = TAB_MISC;
                ImGui::PopStyleColor(3);

                // 在左侧按钮区域添加设置按钮（在draw_Menu函数中）
                // 在MOVEMENT按钮后添加：
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + buttonSpacing);
                ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);
                SetButtonColors(currentTab == TAB_SETTINGS ? ImVec4(0.26f, 0.59f, 0.98f, 0.8f) : ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
                if (ImGui::Button("设置", ImVec2(buttonWidth, buttonHeight))) currentTab = TAB_SETTINGS;
                ImGui::PopStyleColor(3);


                ImGui::PopStyleVar(2); // 结束按钮样式
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(); // 结束左侧背景色

        ImGui::NextColumn();

        // 右侧内容区域 - 添加圆角
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

        switch (currentTab) {
        case TAB_ESP:
            ImGui::BeginChild("ESP", ImVec2(0, 0), true);
            {
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "人物设置");

                // 使用表格布局使选项和颜色选择器对齐
                if (ImGui::BeginTable("ESP_Options", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("选项", ImGuiTableColumnFlags_WidthFixed, 150);
                    ImGui::TableSetupColumn("颜色", ImGuiTableColumnFlags_WidthFixed, 100);

                    // 方框设置
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CustomCheckbox("显示方框", &zeroflick::visuals::box);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(1);
                    CustomColorEdit4("##BoxColor", zeroflick::visuals::BOXcol);
                    ImGui::PopID();

                    // 骨骼设置
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CustomCheckbox("显示骨骼", &zeroflick::visuals::bone);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(2);
                    CustomColorEdit4("##BoneColor", zeroflick::visuals::Bonecol);
                    ImGui::PopID();

                    // 血量设置
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CustomCheckbox("显示血量", &zeroflick::visuals::hp);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(3);
                    CustomColorEdit4("##HPColor", zeroflick::visuals::HPcol);
                    ImGui::PopID();

                    // 名称设置
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CustomCheckbox("显示名称", &zeroflick::visuals::name);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(4);
                    CustomColorEdit4("##NameColor", zeroflick::visuals::namecol);
                    ImGui::PopID();

                    // 武器设置
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    CustomCheckbox("显示武器", &zeroflick::visuals::weapon);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(5);
                    CustomColorEdit4("##WeaponColor", zeroflick::visuals::weaponcol);
                    ImGui::PopID();

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            break;

            // 在AIM标签中添加按键绑定功能
        case TAB_AIM:
            ImGui::BeginChild("AIM", ImVec2(0, 0), true);
            {
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "FOV设置");
                ImGui::Checkbox("FOV", &zeroflick::aim::fov);
                ImGui::SliderFloat("FOV大小", &zeroflick::aim::FOVSize, 0.01f, 100.00f, "%.2f");
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "选地设置");
                ImGui::Checkbox("显示选敌", &zeroflick::aim::inspectEn);
                ImGui::SliderFloat("选敌大小", &zeroflick::aim::inspectEnSize, 0.01f, 5.00f, "%.2f");

                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "自瞄设置");
                ImGui::Checkbox("模拟自瞄", &zeroflick::aim::aimbot);
                ImGui::SliderFloat("平滑系数", &zeroflick::aim::smoothFactorValue, 0.1f, 4.0f, "%.1f");

                // 新增：自瞄按键绑定
                if (ImGui::Button(isBindingAimKey ? "按下按键..." : ("自瞄键: " + std::string(GetKeyName(zeroflick::aim::aimKey))).c_str())) {
                    isBindingAimKey = true;
                }
                ImGui::SeparatorText("自动扳机");
                // 新增：自动开火按键绑定
                ImGui::Checkbox("自动扳机", &zeroflick::aim::autoaim);
                if (ImGui::Button(isBindingTriggerKey ? "按下按键..." : ("自动开火键: " + std::string(GetKeyName(zeroflick::aim::triggerKey))).c_str())) {
                    isBindingTriggerKey = true;
                }

                // 新增：瞄准部位选择
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "瞄准部位");
                ImGui::RadioButton("头部", &zeroflick::aim::aimPart, 0);
                ImGui::SameLine();
                ImGui::RadioButton("胸部", &zeroflick::aim::aimPart, 1);
                ImGui::SameLine();
                ImGui::RadioButton("腹部", &zeroflick::aim::aimPart, 2);

                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "压枪设置");
                ImGui::Checkbox("模拟压枪", &zeroflick::aim::autopunch);
                ImGui::SliderFloat("Y压枪强度", &zeroflick::aim::autopunchsenx, 0.100f, 5.000f, "%.3f");
                ImGui::SliderFloat("X压枪强度", &zeroflick::aim::autopunchseny, 0.100f, 5.000f, "%.3f");
            }
            ImGui::EndChild();
            break;

        case TAB_MISC:
            ImGui::BeginChild("MOVEMENT", ImVec2(0, 0), true);
            {
                ImGui::Checkbox("BHop", &zeroflick::movement::bhop);
            }
            ImGui::EndChild();
            break;

        case TAB_SETTINGS:
            ImGui::BeginChild("Settings", ImVec2(0, 0), true);
            {
                ShowFeedback();
                ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "配置管理");

                // 刷新配置列表按钮
                if (ImGui::Button("刷新配置列表", ImVec2(120, 30))) {
                    RefreshConfigList();
                    SetFeedback("刷新成功", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                }

                ImGui::SameLine();

                // 保存当前配置按钮
                if (ImGui::Button("保存当前配置", ImVec2(120, 30))) {
                    if (selectedConfig >= 0 && selectedConfig < configList.size()) {
                        SaveCurrentConfig(configList[selectedConfig]);
                        SetFeedback(std::string("配置保存成功"), ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    }
                
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 配置列表
                ImGui::Text("可用配置:");
                if (ImGui::BeginListBox("##配置列表", ImVec2(-1, 150))) {
                    for (int i = 0; i < configList.size(); i++) {
                        const bool isSelected = (selectedConfig == i);
                        if (ImGui::Selectable(configList[i].c_str(), isSelected)) {
                            selectedConfig = i;
                        }

                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }

                // 加载选中的配置
                if (selectedConfig >= 0 && selectedConfig < configList.size()) {
                    if (ImGui::Button("加载选中配置", ImVec2(120, 30))) {
                        LoadConfig(configList[selectedConfig]);
                        SetFeedback(std::string("配置加载成功"), ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
                    }

                    ImGui::SameLine();

                    // 删除配置按钮
                    if (ImGui::Button("删除配置", ImVec2(120, 30))) {
                        std::string fileToDelete = configList[selectedConfig] + ".ini";
                        if (fs::exists(fileToDelete)) {
                            fs::remove(fileToDelete);
                            RefreshConfigList();
                            selectedConfig = -1;
                            SetFeedback(std::string("删除成功"), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                        }
                        else {
                            SetFeedback(std::string("删除失败: 文件不存在"), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 新建配置
                ImGui::Text("新建配置:");
                ImGui::InputText("##新配置名称", newConfigName, IM_ARRAYSIZE(newConfigName));

                if (ImGui::Button("创建新配置", ImVec2(120, 30)) && strlen(newConfigName) > 0) {
                    std::string newConfig = newConfigName;
                    if (!newConfig.empty()) {
                        SaveCurrentConfig(newConfig);
                        RefreshConfigList();
                        newConfigName[0] = '\0'; // 清空输入框

                        // 选中新创建的配置
                        auto it = std::find(configList.begin(), configList.end(), newConfig);
                        if (it != configList.end()) {
                            selectedConfig = std::distance(configList.begin(), it);
                        }
                    }
                }
            }
            ImGui::EndChild();
            break;

        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Columns(1); // 结束列布局
    }
    ImGui::End();

    // 恢复样式
    ImGui::PopStyleVar(2);
}