# 《废墟合伙人》(Ruinmates) 代码迁移与重构记录 (Migration Log)

---

## 1. 迁移背景与核心目标

旧工程（`FPS`）存在历史提交庞杂、包含过时 1P 第一人称骨骼依赖、过度复杂的枪械改枪台/配件槽位系统以及非统一的网络同步逻辑。
本次迁移的目标：
1. **彻底解耦**：不再继承旧工程的任何 Git 历史提交，建立纯净全新的独立仓库。
2. **专注于 1–4 人第三人称 PvE 战术撤离核心**：只保留坚固的第三人称动作与枪战底层，去除不需要的冗余系统。
3. **高标准自动化测试覆盖**：迁移过程伴随自动化测试驱动，确保所有迁移出的系统在空资产（Null-Safe）和无网络（Offline）环境下依然健壮。

---

## 2. 批次迁移执行记录

### 第 1 批 (Batch 1)：基础骨架、标签、属性与交互
- **模块骨架建立**：
  - 新建 `Source/RUINMATES.Target.cs`、`Source/RUINMATESEditor.Target.cs`、`Source/RUINMATES/RUINMATES.Build.cs`。
  - 接入 GameplayTags、OnlineSubsystem、GameplayStateTree 模块。
- **纯化武器类型 GameplayTags**：
  - 清理旧工程特化的单一枪械名称标签（如 AK47、AR10），重整为通用分类标签：`Weapon.type.rifle`, `Weapon.type.pistol`, `Weapon.type.sniper`, `Weapon.type.shotgun`, `Weapon.type.lmg`, `Weapon.type.melee`。
- **轻量化 StatsComponent**：
  - 剥离原项目中繁重的 CQB 体力锁死与受击部位微损逻辑，精简为经典的生命值与耐力系统。
  - 修复服务器权限守卫 `CanMutateStats`，确保客户端无法单方篡改耐力或血量。
- **标准化 IInteractable 接口**：
  - 规范化 `CanInteract`、`GetInteractPrompt` 与 `Interact` 虚函数，服务器执行交互生效判定。

### 第 2 批 (Batch 2)：武器实体、数据资产与挂载姿态
- **武器数据资产 `UWeaponData`**：
  - 采用独立的 `UPrimaryDataAsset`，分离武器数值（`CombatParams`）与第三人称蒙太奇（`FMontageData`）。
  - 彻底移除对 1P 网格、InspectMontage1P 等无效依赖。
- **武器 Actor `ABaseWeapon` 与组件解耦**：
  - `UWeaponMountComponent`：提供稳固的插槽挂载逻辑（`AttachToPawn` / `DetachFromPawn`），默认绑定右手掌心插槽 `WeaponSocket_R`。
  - `UWeaponPoseComponent`：将武器类型映射为握持姿态枚举，并输出左手 IK 插槽 `LeftHandSocket`。
- **弹道射线与网络广播**：
  - 开火由服务器打出直线追踪（LineTrace），并通过 `OnRep_FireNotifyId` 驱动双向视觉表现。

### 第 3 批 (Batch 3)：合作会话、组队与漫游切图持久化
- **合作 GameMode 架构**：
  - `ARuinmatesGameMode` 将队伍人数限制标准化为 4 人（`MaxPartySize = 4`），同步设置 `GameSession->MaxPlayers = 4`。
- **漫游旅行存档系统 (SaveTravel)**：
  - 引入 `URuinmatesSessionManager` 与 `URuinmatesSaveGame`。
  - 解决切图重连时队员背包丢失问题：切图前打包快照，进图后 `OnPostLogin` 自动重新装填。

### 第 4 批 (Batch 4)：背包自动生成、瞄准同步与统一动画基类
- **武器背包组件 `UWeaponInventoryComponent`**：
  - 实现开局在服务器 `PossessedBy` 自动为玩家生成默认武器并装备，无需在关卡蓝图手写 Spawn。
- **举镜瞄准复制与状态机**：
  - 角色增加 `bWantsToAim`，提供 `Server_SetWantsToAim` 与 `OnRep_WantsToAim` 同步。
- **统一动画实例 `URuinmatesAnimInstance`**：
  - 解决蓝图直接调用复杂 C++ 查询卡顿的问题，在 C++ 原生线程安全层高速缓存角色运动和瞄准数据。

---

## 3. 自动化测试与质量验收结果

全工程集成自动化测试套件（运行于 `UnrealEditor-Cmd.exe -NullRHI -unattended` 环境）：

| 测试项类别 | 测试用例数 | 状态 | 覆盖特性 |
|---|---|---|---|
| **RuinMates.Batch1.*** | 5 | PASSED | GameplayTags 完整性、武器参数结构、数据资产空指针安全、Stats 复制与服务器权限 |
| **RuinMates.Batch2.*** | 6 | PASSED | 武器姿态映射、插槽回退安全、开火状态流转、空蒙太奇安全 |
| **RuinMates.Batch3.*** | 4 | PASSED | 4人房间上限、本地无损存档读写、切图旅行数据恢复 |
| **RuinMates.Inventory.*** | 4 | PASSED | 默认武器生成、空武器类防护、切枪切换与索引安全 |
| **总计** | **19** | **100% 通过** | *（包含 14 项完全 Success，5 项临时世界销毁警告，0 错误，0 崩溃）* |
