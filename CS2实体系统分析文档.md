# CS2 实体系统逆向分析与修复文档

## 概述

本文档记录了 ZeroFlick 项目中 CS2 (Counter-Strike 2) 游戏实体枚举系统 (`GetBaseEntity`) 的调试与修复过程。原始代码中 `GetBaseEntity` 返回 0，导致 ESP (Wallhack) 功能完全失效。

---

## 问题现象

```
GetBaseEntity(383) = 0x0000000000000000 MISMATCH!
```

- `localPawn` 实体索引为 383
- `GetBaseEntity(383)` 始终返回 0
- ESP 无法枚举任何实体，功能完全失效

---

## 环境信息

| 项目 | 值 |
|------|-----|
| 游戏 | Counter-Strike 2 |
| 模块 | client.dll |
| 实体系统 | CGameEntitySystem |
| 关键偏移 | `dwEntityList` = `0x24E76A0` |

---

## 关键内存结构

### CGameEntitySystem 布局

```
entSystem = *(client.dll + 0x24E76A0)

偏移    内容                        值
+0x00   vtable                      0x00007FFF71ECEF90 (指向 client.dll)
+0x08   (未使用)                    0x0000000000000000
+0x10   实体数组基址 (含标志位)      实体指针数组
+0x18 ~ +0x60  (全部为 0)
```

**唯一有效的字段是 `+0x10`**，它指向一个包含所有游戏实体的扁平数组。

### 实体数组结构

```
entityArrayBase = *(entSystem + 0x10) & ~0x7

每个实体槽位: 0x70 (112 字节)
实体槽位[0x00]: entity 指针 (8 字节, 指向实际 C_BaseEntity 派生对象)
实体槽位[0x08]: vtable 指针
实体槽位[0x10]: handle 值 (低 16 位 = entity index, & 0x7FFF)
实体槽位[0x18]: 0x0000000000000000
实体槽位[0x20]: 标志位
... 其余字段
```

### 实体访问公式

```
entity_ptr = *(*(entSystem + 0x10) & ~0x7 + index * 0x70)
```

- `index`: 实体索引 (0 ~ 0x7FFF)
- `0x70`: 每个实体槽位 112 字节
- `& ~0x7`: 清除基址指针低 3 位可能存在的标志位

---

## 调试过程

### 阶段 1: 初始分析

原始代码使用 **chunk 链表遍历** 方式访问实体：
- 从 `entSystem + 0x10` 读取第一个 chunk
- 遍历 chunk 内节点 (0x70 字节/节点)
- 通过节点的 `+0xC0` 偏移找下一个 chunk

**发现**: 该链表是分配器的 **空闲链表 (free-list)**，只包含已分配的少量实体，不包含索引 383 这样的高索引实体。

### 阶段 2: 尝试公式计算方式

参考项目中 `debug_scanner.cpp` 的另一种实现：
```cpp
chunkAddr = entListBase + 8 * (index >> 9) + 16;
entityAddr = chunkBase + 0x78 * (index & 0x1FF);
```

**结果**: 仍然失败，原因：
- 实体槽位大小错误 (`0x78` 应为 `0x70`)
- `entSystem` 除 `+0x10` 外所有字段均为 0，没有 chunk 表结构

### 阶段 3: 内存扫描定位

在 `entSystem + 0x10` 指向的内存区域扫描 `localPawn` 地址：

```
cleanChunkBase+0x70*383=0x0000041B250CA798, val=0x0000041B6740A000, match=YES
cleanChunkBase+0x78*383=0x0000041B250CB390, val=0x0000000000000000, match=NO
```

**确认**:
- 实体槽位大小 = **0x70** (112 字节)，不是 0x78
- 实体数据直接在 `entSystem + 0x10` 指向的 **单一扁平数组** 中
- 不需要分 chunk，不需要公式 `index >> 9`

### 阶段 4: 最终修复

将 `GetBaseEntity` 重写为直接索引访问：

```cpp
uintptr_t GetBaseEntity(int index, uintptr_t client) {
    auto entSystem = *(uintptr_t*)(client + dwEntityList);
    if (!entSystem) return 0;
    
    uintptr_t entityArrayBase = *(uintptr_t*)(entSystem + 0x10);
    entityArrayBase &= ~0x7ULL;  // 清除标志位
    
    if (!entityArrayBase) return 0;
    
    uintptr_t entityAddr = entityArrayBase + index * 0x70;
    return *(uintptr_t*)(entityAddr);
}
```

---

## 错误 vs 正确对比

| 项目 | 错误实现 | 正确实现 |
|------|----------|----------|
| 实体槽位大小 | 0x78 (120 字节) | **0x70 (112 字节)** |
| 访问方式 | Chunk 链表遍历 | **直接索引访问** |
| Chunk 分块 | `index >> 9` 分 512/块 | **不需要分块** |
| 数组基址 | `entSystem + 0x10` → chunk 链 | `*(entSystem + 0x10) & ~0x7` |
| 实体地址计算 | `chunkBase + inChunkIdx * 0x78` | **`arrayBase + index * 0x70`** |

---

## 相关代码文件

| 文件 | 说明 |
|------|------|
| `ZeroFlick/feature/esp.cpp` | 核心 ESP 代码，包含 `GetBaseEntity`、`GetPlayerPawn`、`draw_esp` 等 |
| `ZeroFlick/feature/debug_scanner.cpp` | 调试扫描器，包含另一种实体遍历实现（也有 0x78 的错误） |
| `ZeroFlick/cs2 dumper/offsets.hpp` | 全局偏移定义 (`dwEntityList` 等) |
| `ZeroFlick/cs2 dumper/client_dll.hpp` | Schema 类偏移定义 (C_BaseEntity, CCSPlayerController 等) |

---

## 关键 Schema 偏移 (已验证)

| 类 | 字段 | 偏移 | 类型 |
|----|------|------|------|
| CCSPlayerController | m_hPlayerPawn | 0x90C | uint32 (CHandle) |
| C_BaseEntity | m_iTeamNum | 0x3EB | uint8 |
| C_BaseEntity | m_iHealth | 0x34C | int32 |
| C_BaseEntity | m_pGameSceneNode | 0x330 | CGameSceneNode* |
| C_BasePlayerPawn | m_vOldOrigin | 0x1390 | Vector3 |
| C_BaseModelEntity | m_vecViewOffset | 0xE70 | Vector3 |

---

## 总结

CS2 的 `CGameEntitySystem` 在本版本中使用**单一扁平数组**存储所有实体，实体通过 `entSystem + 0x10` 获取数组基址，按 `index * 0x70` 直接索引访问。不需要任何 chunk 分块或链表遍历逻辑。

修复后 `GetBaseEntity(383)` 能正确返回 `localPawn` 地址，ESP 实体枚举恢复正常。
