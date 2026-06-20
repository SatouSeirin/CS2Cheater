#include <Windows.h>
#include <d3d11.h>
#include "../minhook_debug_x64/include/MinHook.h"
#include "../imgui_d11/imgui.h"
#include "../imgui_d11/imgui_impl_win32.h"
#include "../imgui_d11/imgui_impl_dx11.h"
#include <thread>
#include "../gui/gui.h"
#include "../feature/esp.h"
#include "../feature/aimbot.h"
#include "../cs2 dumper/offsets.hpp"
#include "../utils/CUserCMD.h"


static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* view = nullptr;
static HWND g_hwnd = nullptr;
void* origin_present = nullptr;

static bool g_showMenu = false;      // 默认关闭菜单，按HOME键打开
static bool g_inited = false;        // ImGui初始化完成标志
static bool g_bPresentHooked = false; // Present Hook是否成功

using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);



WNDPROC origin_wndProc;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

// WndProc Hook：处理HOME键切换菜单和ImGui消息
LRESULT __stdcall WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	// HOME键切换菜单显示
	if (uMsg == WM_KEYDOWN && wParam == VK_HOME) {
		g_showMenu = !g_showMenu;
		if (g_inited) {
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDrawCursor = g_showMenu;
		}
		// 菜单打开时解除鼠标捕获，让玩家能自由移动鼠标
		if (g_showMenu) {
			ClipCursor(nullptr);
			ReleaseCapture();
		}
		return 0;
	}

	// 菜单打开时，让ImGui处理输入
	if (g_showMenu && g_inited) {
		ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
	}

	return CallWindowProc(origin_wndProc, hwnd, uMsg, wParam, lParam);
}

long __stdcall my_present(IDXGISwapChain* _this, UINT a, UINT b) {

	if (!g_inited) {
		// 获取游戏真实SwapChain的设备和上下文
		HRESULT hr = _this->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
		if (FAILED(hr) || !g_pd3dDevice) {
			return ((Present)origin_present)(_this, a, b);
		}
		g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

		// 获取窗口句柄
		DXGI_SWAP_CHAIN_DESC sd;
		_this->GetDesc(&sd);
		g_hwnd = sd.OutputWindow;
		if (!g_hwnd) {
			return ((Present)origin_present)(_this, a, b);
		}

		// 创建渲染目标视图
		ID3D11Texture2D* buf{};
		hr = _this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
		if (FAILED(hr) || !buf) {
			return ((Present)origin_present)(_this, a, b);
		}
		g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &view);
		buf->Release();

		// 安装WndProc Hook
		origin_wndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
		if (!origin_wndProc) {
			return ((Present)origin_present)(_this, a, b);
		}

		// 初始化ImGui
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = 1.0f;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
		io.MouseDrawCursor = false;

		// 尝试加载中文字体，失败则使用默认字体
		ImFontConfig fontConfig;
		fontConfig.MergeMode = false;
		if (!io.Fonts->AddFontFromFileTTF("c:/windows/Fonts/simhei.ttf", 16.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull())) {
			// 备用字体路径
			if (!io.Fonts->AddFontFromFileTTF("c:/windows/Fonts/msyh.ttf", 16.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull())) {
				// 如果都失败，使用默认字体
				io.Fonts->AddFontDefault();
			}
		}

		ImGui_ImplWin32_Init(g_hwnd);
		ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

		gui::Initialize();

		g_inited = true;
	}

	// 重置渲染目标（游戏可能更改了）
	g_pd3dContext->OMSetRenderTargets(1, &view, nullptr);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 显示反馈（在所有界面之上）
	ShowFeedback();

	if (g_showMenu) {
		draw_Menu();
	}
	draw_esp();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return ((Present)origin_present)(_this, a, b);
}

static bool(__fastcall* fnOriginalCreateMove)(void*, int, CUserCMD*) = nullptr;
static bool __fastcall hkCreateMove(void* pCSGOInput, int nSlot, CUserCMD* pcmd) {
	bool bResult = fnOriginalCreateMove(pCSGOInput, nSlot, pcmd);

	// BunnyHop/Autopunch 已移除

	return bResult;
}

// 特征码扫描：在模块中搜索字节模式
static uintptr_t PatternScan(uintptr_t moduleBase, size_t moduleSize, const char* pattern, const char* mask) {
	for (size_t i = 0; i < moduleSize - strlen(mask); i++) {
		bool found = true;
		for (size_t j = 0; j < strlen(mask); j++) {
			if (mask[j] == 'x' && *(reinterpret_cast<uint8_t*>(moduleBase + i + j)) != static_cast<uint8_t>(pattern[j])) {
				found = false;
				break;
			}
		}
		if (found) return moduleBase + i;
	}
	return 0;
}

// 获取模块大小
static size_t GetModuleSize(uintptr_t moduleBase) {
	auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
	auto ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + dosHeader->e_lfanew);
	return ntHeader->OptionalHeader.SizeOfImage;
}

DWORD create(void*) {
	// 等待游戏窗口创建
	while (!GetForegroundWindow()) {
		Sleep(100);
	}

	// 分配控制台用于调试输出
	AllocConsole();
	FILE* file;
	freopen_s(&file, "CONOUT$", "w", stdout);
	printf("[ZeroFlick] DLL injected, initializing...\n");

	// 创建临时D3D11设备来获取Present函数地址（不释放，保持Hook有效）
	const unsigned level_count = 2;
	D3D_FEATURE_LEVEL levels[level_count] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	ID3D11Device* tmpDevice = nullptr;
	IDXGISwapChain* tmpSwapChain = nullptr;

	auto hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		levels,
		level_count,
		D3D11_SDK_VERSION,
		&sd,
		&tmpSwapChain,
		&tmpDevice,
		nullptr,
		nullptr);

	if (tmpSwapChain) {
		// 从虚函数表获取Present函数地址
		auto vtable_ptr = reinterpret_cast<void***>(tmpSwapChain);
		auto vtable = *vtable_ptr;
		auto present = vtable[8];  // IDXGISwapChain::Present = vtable index 8

		printf("[ZeroFlick] Present address: 0x%p\n", present);

		MH_Initialize();
		MH_STATUS status = MH_CreateHook(present, my_present, &origin_present);
		if (status == MH_OK) {
			MH_EnableHook(present);
			g_bPresentHooked = true;
			printf("[ZeroFlick] Present hook installed successfully\n");
		} else {
			printf("[ZeroFlick] Present hook failed: %d\n", status);
		}

		// 释放临时设备和SwapChain（Hook已安装在函数级别，不依赖对象）
		tmpDevice->Release();
		tmpSwapChain->Release();
	} else {
		printf("[ZeroFlick] Failed to create temp D3D11 device for hook\n");
	}

	// 等待 client.dll 加载
	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	if (!client) {
		printf("[ZeroFlick] Waiting for client.dll...\n");
		while (!GetModuleHandle(L"client.dll")) {
			Sleep(500);
		}
		printf("[ZeroFlick] client.dll loaded\n");
	}

	// 获取实际的client.dll基址
	const auto clientBase = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	printf("[ZeroFlick] client.dll base: 0x%p\n", (void*)clientBase);

	// Hook CreateMove
	// 方案：从 dwCSGOInput 获取 CCSGOInput 实例的虚函数表，遍历找到 CreateMove
	// CreateMove 的典型序言特征（多条备选，适应不同版本）:
	//   1. 48 8B C4 4C 89 40 ? 48 89 48 ? 55 53 56 57 48 8D A8  (旧版 asphyxia)
	//   2. 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC (常见 CreateMove)
	//   3. 40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC         (另一种形式)
	// 另外遍历 vtable 条目用启发式方式

	size_t clientSize = GetModuleSize(clientBase);
	printf("[ZeroFlick] client.dll size: 0x%zX\n", clientSize);

	// 多个备选特征码
	struct SigEntry { const char* pattern; const char* mask; const char* name; };
	SigEntry sigs[] = {
		{ "\x48\x8B\xC4\x4C\x89\x40\x00\x48\x89\x48\x00\x55\x53\x56\x57\x48\x8D\xA8", "xxxxxx?xxx?xxxxxxx", "asphyxia-v1" },
		{ "\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\x01", "xxxx?xxxx?xxxx?xxxx?xxx", "cmove-prologue-v2" },
		{ "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x00\x48\x8B\xD9\x48\x8B\x49", "xxxx?xxxx?xxxx?xxxxxxx", "cmove-prologue-v3" },
		{ "\x40\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x81\xEC\x00\x00\x00\x00", "xxxxxxxxxxxxxxxx?xxx", "cmove-frame-v4" },
	};

	uintptr_t pfnCreateMove = 0;

	// 首先尝试特征码扫描
	for (auto& sig : sigs) {
		pfnCreateMove = PatternScan(clientBase, clientSize, sig.pattern, sig.mask);
		if (pfnCreateMove) {
			printf("[ZeroFlick] CreateMove found via %s: 0x%p\n", sig.name, (void*)pfnCreateMove);
			break;
		}
		printf("[ZeroFlick] CreateMove pattern %s: NOT FOUND\n", sig.name);
	}

	// 如果特征码都失败了，尝试从 CCSGOInput 的 vtable 遍历
	if (!pfnCreateMove) {
		printf("[ZeroFlick] All pattern scans failed. Trying vtable scan...\n");
		uintptr_t pCSGOInputPtrAddr = clientBase + cs2_dumper::offsets::client_dll::dwCSGOInput;
		if (!IsBadReadPtr(reinterpret_cast<void*>(pCSGOInputPtrAddr), sizeof(void*))) {
			uintptr_t pCCSGOInput = *reinterpret_cast<uintptr_t*>(pCSGOInputPtrAddr);
			printf("[ZeroFlick] CCSGOInput instance: 0x%p\n", (void*)pCCSGOInput);
			if (pCCSGOInput && !IsBadReadPtr(reinterpret_cast<void*>(pCCSGOInput), sizeof(void*))) {
				uintptr_t vtable = *reinterpret_cast<uintptr_t*>(pCCSGOInput);
				printf("[ZeroFlick] CCSGOInput vtable: 0x%p\n", (void*)vtable);

				// 遍历 vtable 的前 64 个条目
				for (int i = 0; i < 64 && !pfnCreateMove; i++) {
					uintptr_t entryAddr = vtable + i * 8;
					if (IsBadReadPtr(reinterpret_cast<void*>(entryAddr), sizeof(void*))) continue;
					uintptr_t funcPtr = *reinterpret_cast<uintptr_t*>(entryAddr);
					if (!funcPtr || IsBadReadPtr(reinterpret_cast<void*>(funcPtr), 16)) continue;

					// 检查函数指针是否在 client.dll 代码段内
					if (funcPtr < clientBase || funcPtr > clientBase + clientSize) continue;

					// 读取函数序言前16字节
					uint8_t prologue[16];
					memcpy(prologue, reinterpret_cast<void*>(funcPtr), 16);

					// 打印前几个 vtable 条目用于调试
					if (i < 30) {
						printf("[ZeroFlick]   vtable[%d] = 0x%p: %02X %02X %02X %02X %02X %02X %02X %02X ...\n",
							i, (void*)funcPtr,
							prologue[0], prologue[1], prologue[2], prologue[3],
							prologue[4], prologue[5], prologue[6], prologue[7]);
					}

					// 匹配 CreateMove 序言特征: 48 8B C4 (mov rax, rsp)
					// 或者 48 89 5C 24 (mov [rsp+...], rbx)
					if ((prologue[0] == 0x48 && prologue[1] == 0x8B && prologue[2] == 0xC4) ||
						(prologue[0] == 0x48 && prologue[1] == 0x89 && prologue[2] == 0x5C && prologue[3] == 0x24)) {
						printf("[ZeroFlick]   => Candidate at vtable[%d]!\n", i);
						pfnCreateMove = funcPtr;
						break;
					}
				}
			}
		}
	}

	if (pfnCreateMove) {
		MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(pfnCreateMove), hkCreateMove, reinterpret_cast<void**>(&fnOriginalCreateMove));
		if (status == MH_OK) {
			MH_EnableHook(reinterpret_cast<void*>(pfnCreateMove));
			printf("[ZeroFlick] CreateMove hook installed successfully\n");
		} else {
			printf("[ZeroFlick] CreateMove hook failed: %d\n", status);
		}
	} else {
		printf("[ZeroFlick] CreateMove NOT FOUND by any method!\n");
	}

	printf("[ZeroFlick] Initialization complete. Press HOME to toggle menu.\n");

	return 0;
}


BOOL WINAPI DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	if (ul_reason_for_call == 1) {  // DLL_PROCESS_ATTACH
		// 禁用DLL_THREAD_ATTACH/DETACH通知以减轻负载
		DisableThreadLibraryCalls(hModule);

		// 创建主初始化线程（处理Hook和渲染）
		CreateThread(NULL, 0, create, NULL, 0, NULL);

		// triggerbot/autopunch/bhop 已移除，功能线程启动逻辑已不再需要
	}
	return TRUE;
}