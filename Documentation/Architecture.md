# 《废墟合伙人》(Ruinmates) C++ 架构与系统模块说明

---

## 1. 架构总览 (Architectural Overview)

《废墟合伙人》基于分层、组件化（Actor-Component）与接口驱动的设计原则。整个 C++ 运行时严格区分为**核心玩法层**、**网络同步与会话层**、**数据与配置层**以及**自动化测试层**。

```
┌────────────────────────────────────────────────────────┐
│                      网络与游戏会话层                   │
│   ARuinmatesGameMode / ARuinmatesPlayerState          │
│   URuinmatesSessionManager / URuinmatesSaveGame       │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                      角色与控制驱动层                   │
│   ARuinmatesCharacter                                  │
│   ├── UStatsComponent (生命/耐力/权威守护)             │
│   ├── UWeaponMountComponent (插槽绑定/换枪挂点)         │
│   ├── UWeaponPoseComponent (姿态枚举/FABRIK IK)       │
│   └── UWeaponInventoryComponent (背包槽位/默认枪生成)   │
└───────────────────────────┬────────────────────────────┘
                            │
┌───────────────────────────▼────────────────────────────┐
│                      武器与弹道模拟层                   │
│   ABaseWeapon                                          │
│   ├── UWeaponData (DataAsset 数据驱动)                  │
│   ├── FWeaponCombatParams (后坐力/弹道/散布)           │
│   └── FMontageData (3P/1P 动画蒙太奇配置)              │
└────────────────────────────────────────────────────────┘
```

---

## 2. 核心模块与职责划分

### 2.1 角色模块 (`Source/RUINMATES/Character/`)
- **`ARuinmatesCharacter`**：
  - 继承自标准 `ACharacter`。
  - 管理第三人称相机臂（`CameraBoom`）、跟随时相机（`FollowCamera`）。
  - **耐力与冲刺控制**：维护 `bWantsToSprint`，通过 `Server_SetWantsToSprint` 申报，结合 `UStatsComponent` 实现耐力耗尽强制打断。
  - **瞄准控制**：维护 `bWantsToAim` 与 `Server_SetWantsToAim`，同步举镜状态。
  - **武器挂载转发**：持有当前主手武器 `EquippedWeapon`，负责将装备、开火、换弹等动作事件派发到蒙太奇播放器。

### 2.2 属性组件 (`Source/RUINMATES/Stats/`)
- **`UStatsComponent`**：
  - 属性：`CurrentHealth` / `MaxHealth`，`CurrentStamina` / `MaxStamina`，`DeathState`。
  - **网络复制**：
    - `DeathState`：全网全量复制（所有远端客户端需要看到倒地死亡动画）。
    - `Health` / `Stamina`：采用 `COND_OwnerOnly` 仅向本地拥有者复制，节省带宽并隐藏敌人血条数值。
  - **权威防御**：内置 `CanMutateStats()` 守卫，所有消耗、回复与受击仅在服务端结算。

### 2.3 武器与装配系统 (`Source/RUINMATES/Weapon/`)
- **`ABaseWeapon`**：
  - 武器基类 Actor，带有 `USkeletalMeshComponent`（`WeaponMesh`）。
  - 负责换弹计时（`TimerManager`）、射击间隔与循环、开火状态机（`ERuinmatesWeaponStatus`）。
  - **服务器射线命中检测**：`BuildQuery` 排除自身及持有者，沿持有者视线轴打出直线追踪，计算物理材质冲击并下发受击效果。
- **`UWeaponData`**：
  - 纯数据资产（`UPrimaryDataAsset`），解耦枪械数值与网格模型。
  - 包含 `CombatParams`（射速、弹匣容量、单发弹丸数、伤害、散布曲线、后坐力增益）。
  - 包含 `ThirdPersonMontages`（射击、换弹、出枪动画蒙太奇配置）。
- **`UWeaponMountComponent`**：
  - 统一管理武器 Actor 与角色的物理 Attach。默认插槽：右手持枪 `WeaponSocket_R`，背部收纳 `WeaponHolster_R`。
- **`UWeaponPoseComponent`**：
  - 维护角色当前的武器握持姿态枚举（`ERuinmatesWeaponPose`：Unarmed, Rifle, Pistol, Melee）。
  - 输出左手 IK 挂点插槽名称（`LeftHandSocket`），供动画蓝图 FABRIK 节点读取。
- **`UWeaponInventoryComponent`**：
  - 负责角色背包内武器管理。
  - 在角色被控制器 Possession 时，在服务器端自动实例化 `DefaultWeaponClass` 数组中的默认武器，并执行第一把的自动装备（`AutoEquip`）。

### 2.4 动画系统基类 (`Source/RUINMATES/Animation/`)
- **`URuinmatesAnimInstance`**：
  - 作为动画蓝图的 C++ 统一父类。
  - 在 `NativeUpdateAnimation` 中高频提取角色数据并缓存到线程安全成员中：`Speed`, `Direction`, `bIsMoving`, `bIsCrouched`, `bIsAiming`, `AimPitch`, `AimYaw`, `WeaponPoseType`, `bIsReloading`。
  - 使得蓝图 AnimGraph 中可以直接通过快速路径（Fast Path）读取变量，大幅提高动画执行效率。

### 2.5 联机与漫游持久化 (`Source/RUINMATES/Game/` & `Online/`)
- **`ARuinmatesGameMode`**：
  - 基于 Listen-Server 的合作游戏模式。
  - 设定最大队伍人数 `MaxPartySize = 4`，同步配置 `GameSession->MaxPlayers = 4`。
- **`URuinmatesSessionManager`**：
  - 管理局内会话（创建、销毁、加入）。
  - **切图持久化 (SaveTravel)**：在切图前触发 `SaveActivePartyToSaveGame`，通过 `URuinmatesSaveGame` 记录各队员的 `FRuinmatesMemberSave`。在队员于新地图完成 `OnPostLogin` 时，通过 `RestoreMemberFromSaveGame` 无损恢复队员背包与角色状态。

### 2.6 交互系统 (`Source/RUINMATES/Interface/`)
- **`IInteractable`**：
  - 提供标准的 UINTERFACE：`CanInteract`、`GetInteractPrompt`、`Interact`。
  - 交互权威在服务器执行校验，任何客户端试图对不可交互物体发出的操作都会被安全拦截。

---

## 3. 网络复制与权威流程图

```
客户端 (Autonomous Proxy)               服务器权威 (Authority)
     │                                           │
     ├──── [Server RPC] 申请开火 ───────────────>│
     │     ARuinmatesCharacter::Server_TryFire   │ 1. 校验弹药与状态 (CanFire?)
     │                                           │ 2. 消耗 1 发弹药
     │                                           │ 3. 执行 LineTrace 射线检测
     │                                           │ 4. 施加伤害 (ApplyDamage)
     │                                           │ 5. 递增 FireNotifyId
     │                                           │
     │<─── [RepNotify] 状态同步 ─────────────────┤
     │     OnRep_FireNotifyId                    │
     │ 1. 播放本地射击火光与音效                 │
     │ 2. 驱动后坐力视角抬升                     │
     │ 3. 播放 3P 射击蒙太奇                     │
     ▼                                           ▼
```

---

## 4. 自动化测试套件 (`Source/RUINMATES/Private/Tests/`)

- `Batch1MigrationTests.cpp`：覆盖武器 GameplayTags、武器结构体纯参数、数据资产空安全性、Stats 属性与服务器修改权限、Interactable 接口权限。
- `Batch2AnimSelectionTests.cpp`：覆盖武器姿态枚举解析、握持插槽回退机制、武器 Mount 与 Pose 挂载正确性。
- `Batch3SessionTests.cpp`：覆盖 GameMode 4 人上限、SessionManager 本地无损存档创建、切图前后快照恢复。
- `InventoryTests.cpp`：覆盖武器背包组件空类安全、开局自动生成默认武器、切枪索引越界安全。
