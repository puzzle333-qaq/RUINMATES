# 《废墟合伙人》 (Ruinmates)

> A 1–4 player co-op PvE extraction RPG built with Unreal Engine 5.  
> 基于 Unreal Engine 5 打造的 1–4 人联机合作撤离探索角色扮演游戏。

[![Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()
[![Git LFS](https://img.shields.io/badge/Git%20LFS-Enabled-orange.svg)](https://git-lfs.github.com/)

---

## 📖 项目简介

《废墟合伙人》(Ruinmates) 是一款采用虚幻引擎 5 开发的 1–4 人合作 PvE 战术撤离游戏。玩家扮演深入末日废墟的搜刮合伙人，在危险的废墟区域收集物资、击退敌对变异威胁或安保力量，并在倒计时结束前成功撤离。游戏集成了高拟真弹道战斗、耐力与体态管理、战利品搜刮背包，以及完整的房间与局内无缝/非无缝旅行存档继承机制。

---

## 🛠️ 技术特性

- **严格的服务器权威架构（Server-Authoritative）**：生命耐力运算、开火与弹道射线检测、换弹状态机、背包库存变动全部由服务器主导，杜绝作弊与客户端乱序。
- **模块化武器系统**：由 C++ `ABaseWeapon`、`UWeaponData` 数据资产、`UWeaponMountComponent` 挂载组件与 `UWeaponPoseComponent` 姿态系统解耦驱动。
- **多人联机与跨地图存档漫游**：基于 Listen-Server 模式的 `URuinmatesSessionManager`，无缝管理 1–4 人队伍组队、进入危险区及成功撤离后保留玩家物资存档。
- **高拓展性动画实例**：以 `URuinmatesAnimInstance` 为基底，支持多武器姿态切换、AimOffset 瞄准偏移、FABRIK 左手 IK 贴合与蒙太奇分层播放。
- **Git LFS 核心资产保护**：通过 Git Large File Storage (LFS) 严密跟踪手工蓝图、动画状态机与数据资产，避免二进制资产损毁与丢失。

---

## 📂 仓库目录结构

```text
Ruinmates/
├── Source/                  # C++ 源代码（Gameplay、Character、Weapon、Online 等）
│   ├── RUINMATES/           # 主游戏模块
│   ├── RUINMATES.Target.cs
│   └── RUINMATESEditor.Target.cs
├── Config/                  # 虚幻引擎项目配置文件（DefaultEngine.ini 等）
├── Content/                 # 游戏资产（核心手工蓝图纳入 Git LFS 版本控制）
│   └── RUINMATES/           # 核心蓝图（ABP_Unarmed, BP_ThirdPersonCharacter 等）
├── Scripts/                 # 编辑器与动画管线 Python 自动化脚本
├── Documentation/           # 核心开发与设计文档
│   ├── GameDesign.md        # 完整游戏设计与玩法机制方案
│   ├── Architecture.md      # C++ 架构与模块职责说明
│   ├── Migration.md         # 旧工程迁移记录与测试校验
│   └── Animation.md         # ABP 状态机、骨骼插槽与动画接口规范
├── AGENTS.md                # 面向 AI Agent / 协作者的开发约束与红线
├── RUINMATES.uproject       # 虚幻引擎项目工程描述文件
├── .gitattributes           # Git LFS 二进制资产追踪规则
└── .gitignore               # 针对 UE5 开发环境的过滤规则
```

---

## 🚀 快速上手与编译

### 1. 环境依赖
- **操作系统**：Windows 10 / 11 64-bit
- **虚幻引擎**：Unreal Engine 5.8（或 5.5+ 对应环境）
- **编译器**：Visual Studio 2022 (MSVC v143+)
- **Git**：须安装 [Git LFS](https://git-lfs.com/) 并执行 `git lfs install`

### 2. 本地编译
在工程根目录下使用 PowerShell 快速编译编辑器目标：
```powershell
powershell.exe -ExecutionPolicy Bypass -File .\Docs\CompileEditorTarget.ps1
```

### 3. 运行自动化测试套件
```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "$PWD\RUINMATES.uproject" `
  -NullRHI -unattended -NoSplash -ExecCmds="Automation RunFilter RuinMates; Quit"
```

---

## 📚 文档导引

- 🎮 **玩法设计方案**：[Documentation/GameDesign.md](Documentation/GameDesign.md)
- 🏗️ **C++ 架构设计**：[Documentation/Architecture.md](Documentation/Architecture.md)
- 🔄 **代码迁移记录**：[Documentation/Migration.md](Documentation/Migration.md)
- 💃 **动画与骨骼系统**：[Documentation/Animation.md](Documentation/Animation.md)
- 🤖 **Agent 开发准则**：[AGENTS.md](AGENTS.md)
