# 《废墟合伙人》(Ruinmates) 动画与骨骼系统规范 (Animation System)

---

## 1. 动画系统整体设计

《废墟合伙人》采用“**C++ 高速属性缓存 + 动画蓝图状态机/混合空间 + 运行时 IK 修正 + 分层蒙太奇**”的完整第三人称动作管线。

```
┌────────────────────────────────────────────────────────┐
│               ARuinmatesCharacter (Pawn)               │
│   MovementComponent / WeaponPose / EquippedWeapon      │
└───────────────────────────┬────────────────────────────┘
                            │ NativeUpdateAnimation
┌───────────────────────────▼────────────────────────────┐
│              URuinmatesAnimInstance (C++ Base)         │
│   Speed, Direction, bIsAiming, AimPitch/Yaw, PoseType  │
└───────────────────────────┬────────────────────────────┘
                            │ Fast Path (无需调用蓝图函数)
┌───────────────────────────▼────────────────────────────┐
│                 ABP_Unarmed (AnimGraph)                │
│   ├── Main Locomotion State Machine (Idle/Walk/Run)    │
│   ├── BlendSpaces (根据 WeaponPose 姿态切换走位)       │
│   ├── AimOffset (腰射 / 瞄准倾角混合)                  │
│   ├── FABRIK Node (左手 IK 贴合枪身 LeftHandSocket)     │
│   └── Slot 'UpperBody' / 'FullBody' (射击/换弹蒙太奇)  │
└────────────────────────────────────────────────────────┘
```

---

## 2. 动画实例基类 (`URuinmatesAnimInstance`)

所有角色动画蓝图（如 `Content/RUINMATES/Blueprint/Character/ABP_Unarmed.uasset`）均应继承自此类。

### 2.1 导出的核心蓝图变量 (Thread-Safe Fast Path)

| 变量名 | 类型 | 取值范围 | 说明 |
|---|---|---|---|
| `Speed` | `float` | `0.0 .. 600.0` | 角色在水平平面的移动速率（cm/s） |
| `Direction` | `float` | `-180.0 .. 180.0` | 相对视线朝向的移动夹角（度），用于驱动走位混合空间 |
| `bIsMoving` | `bool` | `true / false` | 角色是否有有效移动速度（阈值通常为 > 3.0） |
| `bIsCrouched` | `bool` | `true / false` | 角色当前是否处于蹲伏潜行状态 |
| `bIsAiming` | `bool` | `true / false` | 角色是否正处于举镜瞄准状态 |
| `bIsReloading` | `bool` | `true / false` | 角色当前是否正处于换弹状态 |
| `AimPitch` | `float` | `-90.0 .. 90.0` | 瞄准俯仰角（上下看），用于驱动 AimOffset |
| `AimYaw` | `float` | `-90.0 .. 90.0` | 瞄准水平偏航角（左右看），用于驱动 AimOffset |
| `WeaponPoseType` | `ERuinmatesWeaponPose` | `Unarmed, Rifle, Pistol, Melee` | 当前手持武器姿态类型 |

---

## 3. 骨骼与插槽命名规范 (Skeleton & Sockets)

基础骨架采用 UE 标准 Mannequin 骨骼（`SK_Mannequin`）。

### 3.1 角色骨骼插槽 (Character Skeleton Sockets)
- **`WeaponSocket_R`**（挂载于右手骨骼 `hand_r`）：
  - 主武器就手握持插槽。武器 Actor 的握把原点将精确对齐此插槽。
- **`WeaponHolster_R`**（挂载于脊椎或背部骨骼 `spine_05` / `pelvis`）：
  - 武器收放/副武器背负插槽。

### 3.2 武器骨骼插槽 (Weapon Mesh Sockets)
- **`LeftHandSocket`**（位于武器骨骼网格体前护木）：
  - 角色左手抓取点。动画蓝图中的 FABRIK IK 节点以该插槽为目标（Effector Target），动态将角色的左手掌心对齐到前护木，无需针对不同枪械定制整套握枪动画。

---

## 4. 混合空间与姿态 (Blend Spaces & Poses)

### 4.1 走位混合空间轴向规范
针对 UE Mannequin 坐标系，走位 BlendSpace 标准设定：
- **X 轴 (Horizontal)**：Direction (`-180.0 .. 180.0` 度)。
- **Y 轴 (Vertical)**：Speed (`0.0 .. 600.0` cm/s)。
  - `0.0`：待机 (Idle)
  - `150.0 .. 250.0`：走动 (Walk)
  - `400.0`：常速跑 (Jog)
  - `600.0`：全力冲刺 (Sprint)

### 4.2 瞄准偏移 (Aim Offset)
- **`AimOffset_Hip`**：腰射状态下的轻微视线追踪。
- **`AimOffset_Aim`**：举镜瞄准状态下的精准瞄准偏移。
- 坐标轴：Pitch (`-90.0 .. 90.0`)，Yaw (`-90.0 .. 90.0`)。

---

## 5. 动画蒙太奇与 Slot 分层 (Montages & Slots)

### 5.1 Slot 分组原则
1. **`UpperBody` 插槽**：
   - 过滤脊椎以上骨骼（如 `spine_01` 及以上）。
   - 用于射击（Fire）、换弹（Reload）、出枪收枪（Equip/Unequip）。
   - **特点**：玩家在走动、冲刺或奔跑时播放换弹，下半身依然保持流畅奔跑步伐，不发生滑步。
2. **`FullBody` 插槽**：
   - 全身播放。
   - 用于倒地、死亡、翻滚、大幅攀爬交互动作。
3. **`WeaponSlot` 插槽**：
   - 播放于枪械自身的 SkeletalMeshComponent 上。
   - 驱动枪机拉栓、弹匣拔出与插回。

### 5.2 `FMontageData` 配置项
在 `UWeaponData` 中配置以下蒙太奇指针：
- `EquipMontage`：常规掏枪动作。
- `FirstEquipMontage`：首次检视/上膛出枪动作（为空时自动回退到 `EquipMontage`）。
- `FireMontage`：射击后坐抽搐动作。
- `TacticalReloadMontage`：有余弹战术换弹动作。
- `EmptyReloadMontage`：打空弹匣的空仓挂机换弹动作。

---

## 6. Python 自动化管线脚本

在 `Scripts/SetupWeaponAnimBlueprint.py` 中提供了自动化脚本：
- 自动重设动画蓝图父类为 `URuinmatesAnimInstance`。
- 校验或自动生成基础 AnimGraph 节点。
- 自动化创建 FABRIK IK 变换链。
可由虚幻引擎 Python 控制台一键运行：
```powershell
# 在 UE 编辑器控制台中执行：
py "c:/Users/ASUS/Documents/Unreal Projects/RUINMATES/Scripts/SetupWeaponAnimBlueprint.py"
```
