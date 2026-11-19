# AGENTS.md - Agent 开发规范与架构约束

本文件是面向 AI Coding Agent（以及人类协作者）的项目核心规范文档。在向《废墟合伙人》(Ruinmates) 仓库提交任何代码或修改配置前，请务必严格遵循本规范。

---

## 1. 项目基本信息与技术栈

- **项目名称**：《废墟合伙人》 (Ruinmates)
- **游戏类型**：1–4 人联机 PvE 撤离探索 RPG (Extraction RPG)
- **引擎版本**：Unreal Engine 5.8（兼顾 UE 5.5+ 语法标准）
- **核心模块**：`RUINMATES`（宏：`RUINMATES_API`）
- **目标平台**：Win64 (DirectX 12 / SM6, Substrate, Lumen enabled)
- **网络架构**：带客户端预测与服务器强权威（Server-Authoritative）的 Listen-Server 联机架构

---

## 2. Agent 核心开发戒条（Strict Rules）

### 2.1 服务器权威（Server Authority）
1. **绝对禁止在客户端单方面扣减血量、耐力、弹药或变更背包数据**。
2. 伤害计算、射线命中判定（LineTrace）、开火判定、换弹与拾取交互必须在服务器权威端（`HasAuthority() == true`）执行。
3. 客户端发起意图通过 Server RPC（如 `Server_StartSprint`, `Server_TryFire`）向服务器申报，服务器校验后通过属性复制（`ReplicatedUsing`）下发状态同步。
4. 射线检测（`BuildQuery`）必须由服务器使用 Pawn 的视线/枪口位置执行，坚决不信任客户端上传的 `HitResult`。

### 2.2 资产空指针安全（Null-Safety）
1. C++ 代码在任何网格（`USkeletalMesh`）、动画蒙太奇（`UAnimMontage`）、音效（`USoundBase`）或数据资产（`UWeaponData`）为空（`nullptr`）时，**绝不允许崩溃（Crash）**。
2. 任何资产访问必须包含 `IsValid()` 或空指针短路保护，确保在纯代码自动化测试（如 `-NullRHI -unattended`）中安全运行。
3. 严禁在 C++ 构造函数中使用 `ConstructorHelpers::FObjectFinder` 硬编码资产绝对路径。所有具体资产由蓝图子类或 DataAsset 在编辑器中配置。

### 2.3 隔离旧工程与纯净代码
1. 旧工程 `FPS` 仅作为历史参考，严格只读，**不得继承旧工程提交历史，亦不要引入旧工程专有的冗余枪名、CQB 硬编码或已废弃的配件装配系统**。
2. 保持代码整洁，不随意添加未经讨论的重型插件或实验性依赖。

### 2.4 保留注释与文档完整性
1. 修改现有代码时，必须保留与变更无关的既有注释、Docstring 与格式结构。
2. 新增核心类、RPC、委托或属性时，需添加规范的 Doxygen 风格注释。

---

## 3. 核心架构与模块职责划分

```
Source/RUINMATES/
├── Animation/             # 动画实例基类 URuinmatesAnimInstance 与姿态混合
├── Character/             # 角色基类 ARuinmatesCharacter、移动、耐力控制
├── Game/                  # 游戏模式 ARuinmatesGameMode、玩家状态 ARuinmatesPlayerState
├── Gameplay/              # GameplayTags 常量定义（武器类型等）
├── Interface/             # 交互接口 IInteractable
├── Online/                # 会话管理 URuinmatesSessionManager、存档旅行持久化
├── Stats/                 # 属性组件 UStatsComponent（血量、耐力、死亡状态）
├── Tests/                 # 编辑器 Automation 测试套件
└── Weapon/                # 武器系统：ABaseWeapon、WeaponData、Mount、Pose、Inventory
```

---

## 4. 命名与编码规范

1. **命名规范**：遵循 Unreal Engine 标准。
   - 类前缀：`A`（Actor）、`U`（Object/Component）、`F`（Struct）、`T`（Template）、`I`（Interface）、`E`（Enum）。
   - 布尔变量：以 `b` 开头（如 `bWantsToSprint`, `bIsAiming`）。
   - RPC 函数：以 `Server_` 或 `Client_` 或 `Multicast_` 开头。
   - RepNotify 函数：以 `OnRep_` 开头（如 `OnRep_EquippedWeapon`）。
2. **内存与指针**：
   - 引擎对象必须使用 `TObjectPtr<T>` 或 `UPROPERTY()` 标记，避免被 GC 意外回收或野指针悬空。
   - 弱引用使用 `TWeakObjectPtr<T>`。
3. **日志与断言**：
   - 使用统一日志类别：`UE_LOG(LogTemp, Log/Warning/Error, ...)`。
   - 校验使用 `ensure()` 或 `ensureMsgf()`，避免在游戏运行时使用致命的 `check()` 导致整服中断。

---

## 5. 动画系统与骨骼接口规范

1. **核心骨骼与插槽**：
   - 骨骼：基于 UE 官方 Mannequin 骨骼结构。
   - 右手武器握持插槽：`WeaponSocket_R`（挂载主手武器）。
   - 背部/侧身收枪插槽：`WeaponHolster_R`（挂载副武器/收纳武器）。
   - 左手 IK 插槽：在武器骨骼网格体上配置 `LeftHandSocket`，驱动角色左手 FABRIK IK 贴合枪身前护木。
2. **动画蓝图基类**：
   - 所有角色动画蓝图应继承自 `URuinmatesAnimInstance`。
   - 暴露属性：`Speed`, `Direction`, `bIsMoving`, `bIsCrouched`, `bIsAiming`, `AimPitch`, `AimYaw`, `WeaponPoseType`, `bIsReloading`。
   - 走位 BlendSpace 标准坐标系：X 轴 = Direction (`-180.0 .. 180.0`), Y 轴 = Speed (`0.0 .. 600.0`)。
3. **蒙太奇 Slot 分组**：
   - `UpperBody`：装备、换弹、射击（上半身分层，不打断下半身移动）。
   - `FullBody`：受击倒地、死亡、大幅交互动作。
   - 武器自身骨骼网格体**不再播放 Montage**；`WeaponSlot` 分组仅作历史保留，机械部件保持静态。
4. **武器动画数据（五字段）**：
   - 每把武器只在 `UWeaponData` 配置 `Idle`、`AimOffset`、`Equip`、`Reload`、`Fire` 五个动画资产，详见 `Documentation/WeaponAnimation_Config.md`。
   - 移动、冲刺、蹲伏、跳跃、下落、落地为全局角色动画，禁止逐武器配置。
   - 允许不同武器共享同一动画资产；任何 Montage 可为空，必须空引用安全。
   - 换弹只有一套动画，不区分空仓/非空仓；首次装备与普通装备共用 `Equip`。
   - 新增武器只需新建 `UWeaponData` 并填五个字段，禁止为此修改 `ABP_RuinmatesCombat`。
---

## 6. 自动化测试与构建工作流

在进行任何阶段性交付前，必须在本地通过编译与测试：

1. **编译 Editor Target**：
   ```powershell
   powershell.exe -ExecutionPolicy Bypass -File .\Docs\CompileEditorTarget.ps1
   ```
2. **运行自动化测试**：
   ```powershell
   & "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
     "C:\Users\ASUS\Documents\Unreal Projects\RUINMATES\RUINMATES.uproject" `
     -NullRHI -unattended -NoSplash -ExecCmds="Automation RunFilter RuinMates; Quit" `
     -ReportExportPath="Saved/Automation"
   ```
   *基线要求：所有自动化测试须全部为 Success 或已知的临时世界销毁 Warning，不得有任何 Error 或崩溃。*

---

## 7. 蓝图资产保护与 Git LFS 协作

- 核心蓝图文件位于 `Content/RUINMATES/`，包括：
  - `ABP_Unarmed.uasset`：核心动画蓝图
  - `BP_ThirdPersonCharacter.uasset`：角色蓝图
  - `DA_WeaponDATA.uasset`：枪械配置资产
  - `BP_BaseWeapon.uasset` / `BP_Glock18.uasset`：武器蓝图
- **所有 `.uasset` 和 `.umap` 均由 Git LFS 严格跟踪**。
- 禁止误将 `Content/RUINMATES/` 整体忽略导致手调节点和资产配置丢失。