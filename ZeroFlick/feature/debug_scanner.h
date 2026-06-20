#pragma once

// 运行时内存调试扫描器
// 在控制台打印关键偏移量的实际内存值，帮助诊断功能失效问题

namespace DebugScanner {
    void RunDiagnostics(uintptr_t clientBase);
    void VerifyOffsets(uintptr_t clientBase);
    void ScanEntityList(uintptr_t clientBase);
    void DumpLocalPlayer(uintptr_t clientBase);
}
