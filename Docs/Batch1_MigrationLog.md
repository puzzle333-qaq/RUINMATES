# RUINMATES 第 1 批代码迁移清单

新工程：`C:\Users\ASUS\Documents\Unreal Projects\RUINMATES`（UE 5.8，模块 `RUINMATES`，API 宏 `RUINMATES_API`）
旧工程只读，未改文件 / git / 工作树。本工程当时没有 git，未初始化仓库。

未迁入 `46775be` 的挂载尝试。未做 CoreRedirects 回旧 FPS 类。资产全部留空，引用允许为 null。

## 模块骨架

- `Source/RUINMATES.Target.cs`
- `Source/RUINMATESEditor.Target.cs`
- `Source/RUINMATES/RUINMATES.Build.cs`
- `Source/RUINMATES/RUINMATES.h`
- `Source/RUINMATES/RUINMATES.cpp`
- `RUINMATES.uproject` 增加 Modules 条目

## 块 1 · 武器类型标签

- `Source/RUINMATES/Public/Gameplay/RuinmatesGameplayTags.h`
- `Source/RUINMATES/Private/Gameplay/RuinmatesGameplayTags.cpp`
- 只保留 / 整理武器类型：none / rifle / pistol / melee / shotgun / sniper / lmg
- 不迁配件槽位标签，不迁旧工程包专属枪名标签（AK、X24、AR10 等）

## 块 2 · Interactable

- `Source/RUINMATES/Public/Interface/Interactable.h`
- `Source/RUINMATES/Private/Interface/Interactable.cpp`
- `Source/RUINMATES/Public/World/TestInteractActor.h`
- `Source/RUINMATES/Private/World/TestInteractActor.cpp`
- 保留 CanInteract / GetInteractPrompt / Interact，Interact 仅在服务器生效
- 去掉任务流 / 爆破安放说明与依赖

## 块 3 · StatsComponent

- `Source/RUINMATES/Public/Stats/StatsComponent.h`
- `Source/RUINMATES/Private/Stats/StatsComponent.cpp`
- 生命 / 耐力复制（DeathState 全量，数值 OwnerOnly）
- 去掉 CQB 精简（耐力强制回满）和 HitZoneVitals 接管死亡
- 恢复：冲刺且水平移动时服务器扣耐力，否则回复；生命到 0 进入死亡

## 块 4 · 武器纯参数结构

- `Source/RUINMATES/Public/Weapon/WeaponParams.h`
- 散布 / 射速 / 弹药 / 后坐数据 / 伤害，不含换弹弹匣表现和 1P 视觉后坐

## 块 5 · WeaponData（模型 + 第三人称动画 / Montage）

- `Source/RUINMATES/Public/Weapon/WeaponData.h`
- `Source/RUINMATES/Private/Weapon/WeaponData.cpp`
- 按单把武器的 DataAsset，不是按类型分表的 DA 类别表
- 不含 1P 姿势层、InspectMontage1P、配件 / Mesh1P 依赖

## 测试

- `Source/RUINMATES/Private/Tests/Batch1MigrationTests.cpp`（编辑器自动化测试，Session Frontend → Automation）
- 可将 `ATestInteractActor` 拖进关卡做交互冒烟（无需资产）

## 需要你在编辑器做的最少操作

1. 若编辑器已打开本工程：先关掉 Live Coding / 或关闭编辑器后再编译，编译完成后重新打开工程以加载新 C++ 模块。
2. 重新打开后：Content Browser 即可看到 `StatsComponent`、`Test Interact Actor`、`Weapon Data` 等 C++ 类。
3. 可选：Session Frontend 跑 `RuinMates.Batch1.*` 自动化测试。
4. 资产迁移本批不做，Mesh / Montage 均可保持空。

## 编译状态（2026-11-17 22:14 America/New_York）

第一次 UBT 编译 `RUINMATESEditor Win64 Development` 失败（exit 6，约 8.5 秒）：

`Unable to build while Live Coding is active. Exit the editor and game, or press Ctrl+Alt+F11 if iterating on code in the editor or game`

当时 `UnrealEditor`（PID 44208，打开的是本工程）和 `LiveCodingConsole`（PID 3904）在跑。未关闭编辑器、未杀进程、未使用 Live Coding。

新工程是蓝图第三人称模板，这次才加上 C++ 模块，即使 Live Coding 编译成功，也需要关闭并重新打开编辑器才能加载模块。

请先关闭 Unreal Editor 后再编译。脚本：`Docs\CompileEditorTarget.ps1`。

## 本轮编译与测试（2026-11-17 22:21–22:31 America/New_York）

编辑器已退出后完成。

### 编译

- `Docs/Compile_RUINMATESEditor_Batch1_20261117_2220.log`：第一次完整编 `RUINMATESEditor Win64 Development`，Result: Succeeded，exit 0，约 56.6 秒。工程源码无 error / warning；仅引擎头 C4996 与 IncludeOrderVersion 提示、IncrediBuild license expired（改本地执行）。
- `Docs/Compile_RUINMATESEditor_Batch1_AuthorityFix_20261117_2223.log`：补 `CanMutateStats` 后再编，Succeeded，exit 0，约 21.2 秒。
- `Docs/Compile_RUINMATESEditor_Batch1_InteractTestFix3_20261117_2232.log`：最后一次测试修订后再编，Succeeded，exit 0。

`.uproject` 模块名、`RUINMATES.Target.cs` / `RUINMATESEditor.Target.cs`、`RUINMATES.Build.cs` 一致，均为模块 `RUINMATES`，UE 5.8，未升 5.9。

### 无界面自动化（UnrealEditor-Cmd -NullRHI -unattended）

过滤 `RuinMates.Batch1`，报告在 `Saved/Automation/Batch1`，完整日志在 Docs。

通过 5 项：WeaponTypeTags、WeaponParams、WeaponDataNullSafe、StatsComponent、StatsAuthority。

失败 1 项：InteractAuthority。进程 exit 255 / TEST COMPLETE -1。
直接调用 `CanInteract_Implementation` 为真，`IInteractable::Execute_CanInteract/Interact` 未走到 C++ 实现（返回默认 false，计数仍为 0）。服务器权限在 `Interact_Implementation` 里已做，但 Execute_ 路径本轮未验收通过。未为过测试放宽断言或删功能。

### 源码审查

- 工程 Source 中未检出 Mesh1P / SyncBody / 任务流 / 配件系统依赖。
- Stats：耐力 Tick 仅服务器；生命/耐力/最大值为 OwnerOnly 复制，DeathState 全量复制；变更接口现要求 `CanMutateStats()`（无 Owner 的测试对象或拥有者有 Authority 才可改）。
- 武器数据空引用安全测试已通过。
- 未初始化 git，未进第 2 批，未迁资产。

## 第 1 批验收（2026-11-17 22:47 America/New_York）

编辑器未打开。在 `InteractAuthority` 测试里用 `FEditorScriptExecutionGuard` 包住 `IInteractable::Execute_*` 调用：这只放开编辑器世界里未初始化 Actor 的 `ProcessEvent` 拦截，不改变 `HasAuthority()`，也不会绕过 `Interact_Implementation` 的服务器权限检查。产品接口本身未改。

断言仍要求实现真正执行，而不是默认 false 假通过：

- `Execute_CanInteract(nullptr)` 必须返回 false，且 `CanInteractImplCalls` 增加。
- `Execute_CanInteract(Pawn)` 必须返回 true，且计数再次增加。
- 服务器 `Execute_Interact` 使 `InteractCount == 1`。
- `ROLE_SimulatedProxy` 下再次 `Execute_Interact`（同样在 Guard 内，实现会跑到）后计数仍为 1。

### 编译

- `Docs/Compile_RUINMATESEditor_Batch1_InteractGuard_20261117_2247.log`：`RUINMATESEditor Win64 Development`，Result: Succeeded，exit 0，约 5.0 秒。

### 无界面自动化

- 过滤 `RuinMates.Batch1`，日志 `Docs/Automation_RuinMates_Batch1_20261117_2247.log`，报告 `Docs/Automation_RuinMates_Batch1_20261117_2247_index.json`（出自 `Saved/Automation/Batch1`）。
- **通过 6 项，失败 0 项**：InteractAuthority、StatsAuthority、StatsComponent、WeaponDataNullSafe、WeaponParams、WeaponTypeTags。进程 exit 0 / `TEST COMPLETE. EXIT CODE: 0`。
- InteractAuthority 无 error / warning 条目；实现调用计数断言已通过。

### 状态

- 旧工程仍只读，远端 tip 仍是 `46775be`，未做 revert / 推送，也未假装已清理。
- 工程 Source 中此前已确认无 Mesh1P / SyncBody / 任务流 / 配件系统依赖。
- 资产仍留空。未进第 2 批：完整方案第 0 步要求 tip 回到 `9aa225f` 后再抽武器代码。
