# RUINMATES 蓝图 / 编辑器交接单

更新时间：2026-11-18 05:50（America/New_York）  
本地提交：在 `10a9430`（蓝图交接单）之上增加 `UWeaponInventoryComponent` 自动生成默认武器。未推送，没有远程仓库。提交哈希以仓库 tip 为准。

## 先读这段

- **代码通过**：`RUINMATESEditor Win64 Development` 完整编译成功；`RuinMates` 自动化 **19/19** 通过（14 项 Success，5 项 Success with warnings）。那 5 项警告来自既有第 2 批以及新的 Inventory 测试清理临时世界时的 `UWorld::DestroyActor: World has no context`，不是功能失败。
- **运行未测**：单人步枪（M1）、双人一致性（M2）、组队进危险区再回基地并保留存档（M3）都还没有在编辑器或打包客户端里实测。空资产不等于里程碑通过。
- C++ 在资产为 null 时是安全的；基本自动化不依赖真实武器网格或 Montage。
- 当前在线层是引擎自带 **OnlineSubsystemNull**。`OpenPlatformInvite()` 会返回 false，**没有可用的 EOS/Steam 房间**，不要把它当成已经能开平台房间。
- 旧工程 `FPS` 只读，不要从失败提交 `46775be` 迁挂载改动。迁资产前先看依赖列表。
- 本清单只写需要你在编辑器里做的事。已经能在 C++ 里自动完成的装备绑定、默认武器生成、服务器射线、换弹、切图前存档和登录后恢复，没有留给蓝图。

图例：

- **必需**：不完成则关卡里玩不成对应功能。
- **可选**：没有也能编译、也能先做单人冒烟，但体验不完整。
- **代码通过 / 运行未测**：功能已在自动化里覆盖，或只在代码层存在、尚未实机验证。

---

## 1. 游戏模式和默认 Pawn（必需，运行未测）

`Config/DefaultEngine.ini` **没有被改成新的 GameMode**，现在仍是第三人称模板：

- `GameDefaultMap` / `EditorStartupMap` = `/Game/ThirdPerson/Lvl_ThirdPerson`
- `GlobalDefaultGameMode` = `/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C`

请任选一种，不要两种各改一半：

1. 把 `BP_ThirdPersonGameMode` 的父类改成 `ARuinmatesGameMode`，或  
2. 在项目设置 / 地图 GameMode Override 里直接指定 `ARuinmatesGameMode`（或它的蓝图子类）。

`ARuinmatesGameMode` 构造时已经设置：

- `DefaultPawnClass` = `ARuinmatesCharacter`
- `PlayerStateClass` = `ARuinmatesPlayerState`
- `MaxPartySize` = 4  
- `InitGame` 里把 `GameSession->MaxPlayers` 设为 `MaxPartySize`（UE 5.8 的人数上限在 `AGameSession` 上，不在 `AGameMode` 上）

角色蓝图：

- **必需**：把 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` 的父类改成 `ARuinmatesCharacter`，**或者**新建一个以 `ARuinmatesCharacter` 为父类的角色蓝图，并把 GameMode 的 Default Pawn 指过去。
- 重父类后核对 Mesh、AnimBP、相机还在。C++ 已经创建了 `CameraBoom`、`FollowCamera`、`Stats`、`WeaponPose`、`WeaponInventory` 子对象；不要再重复加一套同名组件。
- 模板自带的移动输入可以保留；武器和冲刺必须接到下面第 2 节的 C++ 函数。

---

## 2. 输入接线（必需，运行未测）

`ARuinmatesCharacter` **没有** `SetupPlayerInputComponent`，C++ 不会自动绑 Enhanced Input。在角色蓝图或 Player Controller 的 Input Mapping Context 里接线：

| 输入意图 | 调用的 C++ 函数 | 说明 |
|---|---|---|
| 冲刺按下 | `ARuinmatesCharacter::StartSprint()` | 耐力耗尽会由 `UStatsComponent` 自动停冲刺 |
| 冲刺松开 | `ARuinmatesCharacter::StopSprint()` | |
| 蹲下切换 | `ARuinmatesCharacter::ToggleCrouch()` | |
| 开火 | `ARuinmatesCharacter::TryFireWeapon()` | 内部转到装备武器的 `TryFire()`；服务器自己打射线，不采用客户端 HitResult |
| 换弹 | `ARuinmatesCharacter::TryReloadWeapon()` | 服务器权威换弹，换弹期间 `CanFire()` 为 false |
| 装备（调试或拾取成功后） | `ARuinmatesCharacter::EquipWeapon(ABaseWeapon*)` | 默认武器开局会自动装备，不必再在 BeginPlay 里手写生成。拾取或调试仍可调用。服务器执行；会绑挂载和 pose，并播放 3P 装备 Montage（资产存在时） |
| 卸下（可选调试键） | `ARuinmatesCharacter::UnequipWeapon()` | 可选 |
| 切枪（可选） | `UWeaponInventoryComponent::CycleNextWeapon()` 或 `EquipWeaponAtIndex(int32)` | 仍走角色的 `EquipWeapon`，不是第二套装备路径。C++ 没有自动绑键。 |

移动、视角可以继续用第三人称模板已有的 IMC / IA。不必为了迁移去改 C++ 输入。

交互如需接：接口是 `IInteractable` 的 `CanInteract` / `GetInteractPrompt` / `Interact`。权限在服务器上检查。这不是步枪游玩的必需项。

---

## 3. 武器蓝图、WeaponData 和默认武器列表（必需，运行未测）

开局武器由角色上的 `UWeaponInventoryComponent` 在服务器 `PossessedBy` 时自动生成，并调用已有的 `EquipWeapon` 装备第一把。挂点仍从 `UWeaponData` 读取。**不要**再在 `BP_ThirdPersonCharacter` 的 Event Graph 里手写 `Spawn Actor from Class` + `EquipWeapon`。

请按这个面板路径赋值（名称必须对上，不要猜）：

1. 新建 `ABaseWeapon` 的蓝图子类（例如 `BP_Rifle`）。
2. 打开该武器蓝图，在 **Class Defaults** 里找到 **Weapon Data**（C++ 属性名 `WeaponData`），把你做的 `UWeaponData` 资产赋上去，Compile、Save。没有这份数据时不会崩溃，但没有网格，挂点也会回退到默认的 `hand_r`。
3. 打开 `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`（父类必须已经是 `ARuinmatesCharacter`，否则看不到这个组件）。在 **Class Defaults** 中选中组件 **Weapon Inventory**（C++ 组件名 `WeaponInventory`，类 `UWeaponInventoryComponent`），找到属性 **Default Weapon Class**（C++ 名 `DefaultWeaponClass`），把你的 `ABaseWeapon` 子蓝图加进数组。Compile、Save，然后 Play。

第一把会自动装备；数组里后面的武器会生成但先藏着，可用可选的 `CycleNextWeapon` / `EquipWeaponAtIndex` 切换。不用在 BeginPlay 里设插槽。

旧工程的武器仓库 UI、改枪台、配件预设和 `UWarehouseLoadoutSaveGame` **没有迁过来**。第 3 批存档里的 `FRuinmatesMemberSave::EquippedLoadoutId` 只是字符串占位，没有旧的 `FLoadoutPreset` / `GetSubmittedLoadout()` 武器类列表，所以现在 `SpawnInventory` 只用 `DefaultWeaponClass`。以后若存档补上武器类字段，再接到 `RebuildInventory`。

`UWeaponData` 需要你填的字段：

| 字段 | 作用 |
|---|---|
| `WeaponType` | Gameplay Tag。步枪用 `Weapon.type.rifle`，狙击用 `Weapon.type.sniper`，近战用 `Weapon.type.melee`。另外还有 `Weapon.type.pistol` / `shotgun` / `lmg` / `none`，当前种类枚举只会把狙击和近战区分出来，其余按步枪处理。 |
| `WeaponMesh` | 武器骨骼网格。可空；空则没有可见枪，但代码和测试仍安全。 |
| `MeshAttachSocket` | 角色身上的挂点。会写入 `UWeaponMountComponent::Mesh3PAttachSocket` 和 `DefaultGripSocket`。留空则默认 `hand_r`。 |
| `LeftHandSocket` | 武器网格上的左手 IK 插槽，写入 `UWeaponPoseComponent::SetPreferredFabrikSocket`。 |
| `CombatParams` | `FWeaponCombatParams`：`Fire`（`FireType` / `FireTime` 等）、`Spread.HipFireSpread`、`Ammo`（`MagCapacity`、`StartingAmmoInMag`、`StartingCarriedAmmo`、`PelletsPerShot`）、`Recoil`、`Damage`。 |
| `ThirdPersonAnims` | `FPlayerAnim` 走位/瞄准动画。给 AnimBP 图层用。可选，空引用安全。 |
| `ThirdPersonMontages` | `FMontageData`：角色身体的装备 / 开火 / 空仓换弹 / 战术换弹 Montage。C++ 会在装备、开火、换弹时自动 `PlayAnimMontage`。 |
| `WeaponMontages` | 同一套 `FMontageData`，播在武器网格上。可选。 |

`FMontageData` 字段：`EquipMontage`、`FirstEquipMontage`（空则回退到 `EquipMontage`）、`FireMontage`、`EmptyReloadMontage`、`TacticalReloadMontage`，以及可选的 `FireMontages` / `EquipMontages` 随机变体。解析函数在资产为 null 时返回 nullptr，不会崩。

武器演员上可调、一般不用改的默认值：

- `ReloadFallbackDuration`（默认 1.5；没有换弹 Montage 时用它，0 表示立即完成）
- `MuzzleSocketName`（默认 `Muzzle`）
- `TraceChannel` = `ECC_Visibility`，`TraceLength` = 10000，`MaxViewOriginDistance` = 1000

不要做：1P 网格、配件系统、CQB、把整套旧 `Weapon` / `CombatComponent` 搬过来。近战只保留共用接口，`UsesMeleeSweep` 时当前 hitscan 直接返回，没有挥砍实现。

---

## 4. AnimBP FABRIK（必需，运行未测）

左手 IK **不会**在 C++ 里改骨骼；只更新姿态组件里的目标和权重，必须由第三人称 AnimBP 来读。

角色用 `GetWeaponPose()` 拿到 `UWeaponPoseComponent`。

| AnimBP 用法 | C++ |
|---|---|
| FABRIK Effector Transform | `GetFabrikSocketTransform()`，也是 `FabrikSocketTransform` |
| FABRIK Alpha | `GetFabrikAlpha()`，也是 `FabrikAlpha` |

**坐标空间（容易接错）：** `WriteHandSpace()` 用 `USkeletalMeshComponent::TransformToBoneSpace("hand_r", ...)`。给 FABRIK 的是 **`hand_r` 骨骼空间**，不是世界空间，也不是组件空间。FABRIK 节点的 Effector Transform Space 必须对上 `hand_r`。角色网格上没有 `hand_r` 时，组件会把 transform 清成 Identity、alpha 为 0。

插槽查找只使用武器网格上真实存在的左手插槽，**不会**退回枪口。优先顺序：

1. `UWeaponData::LeftHandSocket`（经 `SetPreferredFabrikSocket`）
2. `GripSocket`
3. `LeftHand`
4. `LeftHandSocket`
5. `IK_Hand_L`
6. `hand_l_socket`
7. `ik_hand_l`

换弹时默认关闭左手 IK：`bDisableLeftHandIKDuringReload = true`，`SetReloading(true)` 后 `FabrikAlpha` 会收到 0。可选：把 `DisableLeftHandIKCurveName` 设成 Montage 曲线名，用曲线再压一次 alpha。`LeftHandIKBlendSpeed` 默认 12。

近战（`ERuinmatesWeaponKind::Melee`）不启用左手 FABRIK。

---

## 5. 挂载插槽和资产 Migrate（必需网格后才能看见枪，运行未测）

C++ 挂载已经在 `EquipWeapon` 里自动调用 `UWeaponMountComponent::AttachToOwningPawn()`，不用在蓝图里再 Attach。

- 默认挂点：`Mesh3PAttachSocket` = `hand_r`（可用 `UWeaponData::MeshAttachSocket` 覆盖）
- 校准偏移：`Mesh3PAttachTransform`，每次抓握重新套用，不会累加。可在武器蓝图里微调。
- 回退抓握名：`DefaultGripSocket`，默认也是 `hand_r`。
- 测试用过的人模：`/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`

从旧工程 **Migrate** 前先打开引用查看器，只勾步枪需要的网格、插槽、第三人称 Montage / 动画，避免把旧大厅、射击场、1P、CQB、任务流或失败的 `46775be` 挂载尝试带进来。迁完把引用填到 `UWeaponData`，不要留下指回旧工程路径、一打开就坏的硬链接。

没有网格和 Montage 时，代码仍可编译、自动化仍可通过，但关卡里看不到枪、也没有开火/换弹动作。

---

## 6. 联机、地图和打包（联机部分可选到你开始做听服务器为止；平台房间目前不可用）

已经在工程里配好、你不用再装插件：

- `RUINMATES.Build.cs`：`OnlineSubsystem`、`OnlineSubsystemUtils`
- `RUINMATES.uproject`：启用引擎自带 `OnlineSubsystem`、`OnlineSubsystemUtils`、`OnlineSubsystemNull`
- `DefaultEngine.ini`：`[OnlineSubsystem] DefaultPlatformService=Null`

**不要**在没有 SDK 和凭据时打开 EOS / Steam 插件，也不要假装已经有平台房间。

`URuinmatesSessionSubsystem`（Game Instance 子系统）你需要配置或调用的部分：

| 名称 | 做什么 |
|---|---|
| `BaseMapName` / `DangerMapName` | **必需**（一旦要切图）。在该子系统的 Class Defaults 里填已打包的地图名。空名字会拒绝旅行。 |
| `HostListenParty()` | 准备 1–4 人本地 Listen Server，**不会自己切图**，也不是平台房间。 |
| `TravelToDangerZone()` / `TravelToBase()` | 仅主机、且已组队。会先 `CapturePartyFromWorld` + `SaveCampaign`，再 `ServerTravel` 到 `地图名?listen`。 |
| `OpenPlatformInvite()` / `CanOpenPlatformInvite()` | Null 层下为 false。即使以后检测到非 Null 在线层，邀请 overlay 也还没接线，仍会返回 false。 |
| `GetStatusText()` | 可读的中文状态，适合做临时 UI。 |

打包时把基地和危险区地图都加进 Maps to Cook / List of maps to include。人数上限 4，Listen Server，没有 Dedicated Server 路径。

存档（C++ 已接，不必在蓝图里重复实现切图读写）：

- 槽名默认 `RuinMatesCampaign`（`URuinmatesSaveSubsystem::SaveSlotName`）
- `Initialize` 时若槽存在会 `LoadCampaign()`
- 切图前主机自动抓取队员并 `SaveCampaign()`
- `ARuinmatesGameMode::PostLogin` 调用 `RestoreOrAssignMember`，用 UniqueNetId 或 `PlayerId:N` 恢复 `ARuinmatesPlayerState` 的 `StableMemberId`、`CampaignLevel`、`TalentIds`、`EquippedLoadoutId`
- 客户端 `CanMutateSave()` 为 false
- 蓝图如需改资金或进度，可用 `SetSharedFunds` / `SetMemberProgress` / `GetSharedFunds` / `GetMemberProgress`，然后可手动再调 `SaveCampaign()`（可选，例如在基地商店里）

以上会话、切图、存档路径都是 **代码通过、联机与旅行运行未测**。

---

## 7. 建议的编辑器自测步骤（你来做，完成后才谈 M1/M2/M3）

这些步骤通过之前，不要把 M1/M2/M3 标成完成。

1. **单人冒烟（M1 候选）**：重父类、接输入，并在角色 **Weapon Inventory** 的 **Default Weapon Class** 里加上已赋 `Weapon Data`、网格和 Montage 的武器蓝图后，直接 PIE。第一把应自动生成并挂在 `hand_r`（或你在 `MeshAttachSocket` 里设的插槽），左手 FABRIK 跟着 `LeftHandSocket`，开火消耗弹药，换弹播放 Montage 并填弹。不必再在 BeginPlay 里 Spawn + Equip。空资产时不应崩溃。
2. **听服务器两人（M2 候选）**：用 `HostListenParty` 开本地多人，第二台客户端加入，核对双方看到同一把枪、同一弹药/换弹状态。当前没有平台邀请。
3. **基地 → 危险区 → 基地（M3 候选）**：填好 `BaseMapName` / `DangerMapName` 并打包这两张图，改资金或等级后切图，再重启编辑器/游戏，确认 `RuinMatesCampaign` 槽里的共享资金和成员进度还在。

---

## 8. 明确不做 / 仍然阻塞的事项

- 背包、搜刮、AI、废墟撤离玩法、商店最小集：不在这次迁移范围。
- 近战挥砍、梦想武器、1P、配件、旧大厅/射击场流程：不迁。
- EOS / Steam 登录、SDK、凭据、真实房间：未接。缺这些就不能声称可以开平台房间。
- 远程仓库：由你稍后创建和推送，本地不要用 `gh`。
- 需要你完成的阻塞：第 1–5 节的蓝图父类、输入、武器数据、AnimBP FABRIK、网格/Montage Migrate。第 6 节地图名是切图的阻塞。M1/M2/M3 的运行验收是独立阻塞，不能用自动化通过代替。

---

## 9. 换弹滑步修复：上半身分层（2026-11-18，PIE 已确认换弹正常）

### 根因

`ABP_Unarmed` 的 AnimGraph 原来是一条直线，Slot 直接盖在状态机输出上：

```
Control Rig ← Slot 'DefaultSlot' ← State Machine 'Main States'
```

整张图里没有 `Layered Blend Per Bone`，所以 `DefaultSlot` 拿到 Montage 后是**整身替换**。
换弹 Montage（`MM_Pistol_Reload_Montage`，2.0s，槽位 `DefaultSlot`）是全身片段，连腿一起换掉，
换弹期间下半身不动而人还在移动，于是滑步。

### 修复（只保留这一个改动）

状态机输出先存成缓存姿态，再由两路读取（不走两次状态机），Slot 与分层节点都读这份缓存：

```
State Machine 'Main States'
   └─> Save Cached Pose 'RuinmatesBodyPose'
          ├─> Use Cached Pose ──> Layered Blend Per Bone .BasePose   （pelvis / 腿，来自走位）
          └─> Use Cached Pose ──> Slot 'DefaultSlot' ──> Layered .BlendPose0 （spine_01 及子骨骼）
                                                       └─> Control Rig ──> 后续原链路
```

分层节点参数：`Blend Mode = BranchFilter`，`BranchFilters = spine_01`、`BlendDepth = 0`，
`BlendWeights = 1.0`。

- 没 Montage 时 Slot 直通，idle 与原始资产逐骨骼一致（见下面实测数字）。
- 有 Montage 时只覆盖 spine_01 分支，pelvis 和腿继续走走位动画。
- 资产改动只有 `Content/RUINMATES/Blueprint/Character/ABP_Unarmed.uasset`。
  Montage 资产没动，槽位仍是 `DefaultSlot`。
- 改前备份：`Saved/AnimBackup/ABP_Unarmed_before_upperbody.uasset`（原始，未分层），
  回滚直接覆盖回 `Content/RUINMATES/Blueprint/Character/ABP_Unarmed.uasset`。
- 这次没有留 C++ 改动，也没有留诊断用的 Editor 命令舱。

**为什么必须缓存，不能把状态机接两路**：`FAnimNode_StateMachine` 一帧内被求值两次时，
第二次返回的是退化姿态（参考姿态）。第一版把状态机同时接到 Slot 源和分层 BasePose，
结果 idle 上半身冻在参考姿态（用户看到的"变 A-pose"）。`Save/Use Cached Pose` 只求值一次，
两路各自拷贝，问题消失。

### 实测数据（无界面姿态探针，比原始资产对照）

`upperarm_r / lowerarm_r / spine_03 / neck_01 / thigh_l` 相对参考姿态的角度差：

| 场景 | 原始资产 | 第一版（状态机接两路，坏） | 现在（缓存姿态） |
|---|---|---|---|
| idle | 31.58 / 36.34 / 23.71 / 21.48 / 18.15 | 22.05 / 22.05 / 22.05 / 22.05 / 18.15（上半身冻结） | 31.58 / 36.34 / 23.71 / 21.48 / 18.15 |
| 换弹 | 66.31 / 86.03 / 4.58 / 9.72 / **23.65** | 55.19 / 72.70 / 17.98 / 19.52 / 18.19 | 55.19 / 72.70 / 17.98 / 19.52 / **18.19** |

关键看 `thigh_l`：原始资产换弹时是 23.65（腿被换弹片段替换），现在是 18.19（≈ idle 的 18.15，
腿保持走位），idle 的五个数值与原始资产完全一致。

### 注意

- 分层作用在全身 Slot 上，开火 Montage 走同一个 Slot，所以开火也只覆盖上半身。
  开火手感如果还要调，是独立的一轮，不在这次范围内。
- 装备 Montage 仍是空的（`3P.Equip = <null>`），所以没有拔枪动作。
- 以后要再改这张图：编辑器里打开 `ABP_Unarmed` → AnimGraph，确认
  `Save Cached Pose 'RuinmatesBodyPose'` 有两路 `Use Cached Pose`，
  `Layered Blend Per Bone` 的 Base Pose 接腿那路、Blend Pose 0 接 Slot，分支固定 `spine_01`。
  不要把同一个状态机直接接两路。

### 自动化命令（顺带更正）

`RuinMates` 现在是 21 项，UE 5.8 里 `Automation RunFilter RuinMates` 已经不认（会报
`RuinMates is not a valid flag to filter on`），要用：

```
-ExecCmds="Automation RunTests RuinMates; Quit" -TestExit="Automation Test Queue Empty"
```

21/21 Success。
