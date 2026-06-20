#include "Windows.h"
#include "./bhop.h"
#include "esp.h"
#include "../cs2 dumper/offsets.hpp"
#include "../cs2 dumper/client_dll.hpp"
#include "../gui/gui.h"

void bhop(CUserCMD* pcmd)
{
	if (!zeroflick::movement::bhop || !pcmd)
		return;

	const auto client = reinterpret_cast<uintptr_t>(GetModuleHandle(L"client.dll"));
	if (!client)
		return;

	auto local_ctrl = *reinterpret_cast<uintptr_t*>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
	if (!local_ctrl)
		return;

	auto local_hpawn = *reinterpret_cast<uint32_t*>(local_ctrl + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);
	if (local_hpawn == 0xFFFFFFFF)
		return;

	auto localpawn = GetBaseEntityFromHandle(local_hpawn, client);
	if (!localpawn)
		return;

	uint32_t m_fflags = *reinterpret_cast<uint32_t*>(localpawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags);

	// FL_ONGROUND = (1 << 0), 在地面时松开跳跃键
	if (m_fflags & (1 << 0))
		pcmd->nButtons.nValue &= ~IN_JUMP;

}