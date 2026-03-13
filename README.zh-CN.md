# HOT4D-C4D2026Plus

本仓库中的 HOT4D 已经**完成对 Cinema 4D 2026 的适配并可正常工作**。

这个仓库是对原始 HOT4D 项目的延续 / 适配版本，目标是在尽量保留原始架构和许可证的前提下，让插件能够在更新版本的 Cinema 4D SDK 上继续使用。

## 上游来源

本项目基于原始 HOT4D 项目：

- 上游仓库：https://github.com/Valkaari/HOT4D
- 原始作者 / 仓库所有者：Valkaari

HOT4D 本身基于 Houdini Ocean Toolkit 以及原项目中引用的相关上游工作。

## 许可证

本仓库继续遵循上游许可证，使用 **GNU GPL v2.0**。

参见：

- `LICENSE.txt`：保留的许可证文本
- 上游仓库：原始声明与归属信息

## 当前状态

**Cinema 4D 2026 适配：已完成，可正常使用。**

当前这次移植已经验证到以下程度：

- 插件可以在当前 Cinema 4D 2026 SDK 构建链路下成功编译
- ocean simulation 的创建路径已经恢复正常
- deformer 可以在时间线播放时正确持续更新
- Jacobian / Foam 相关功能已恢复到可工作状态
- Point Selection 的崩溃路径已修复
- Maxon registration bootstrap 已修复，并且已经固化为可重复构建方案

一句话说：这个仓库已经不只是“研究/排障分支”，而是一个**可工作的 HOT4D for Cinema 4D 2026 适配版本**。

## 2026 适配中完成的关键修复

这次适配中完成的重要兼容性和稳定性修复包括：

- 恢复 `OceanSimulation_impl.cpp` 中真实的 ocean 实现路径
- 修复 Maxon registration bootstrap，使 `Ocean().Create()` 再次可用
- 通过 `project/CMakeLists.txt` 将注册修复固化为可重复构建规则
- 通过 execution chain 修复 deformer 播放时不自动更新的问题
- 修复 Jacobian / Point Selection 路径中并行写 SelectionTag 导致的不安全 / 崩溃问题
- 恢复 Jacobian / Foam 标签创建及其相关 UI 行为
- 在项目稳定后清理掉临时诊断 scaffold

如果你想看更工程化、更细的移植经验总结，请看：

- `BUILDING.md`

## 仓库结构

整体代码结构尽量保持接近上游：

- `source/` —— 插件与 ocean simulation 源码
- `res/` —— Cinema 4D 资源、description、字符串、图标等
- `project/` —— Cinema 4D 模块构建元数据，以及项目级 CMake 覆盖
- `scripts/` —— 基于 SDK 的本地辅助脚本
- `docs/` —— 迁移分析和补充说明文档

## 构建方式

推荐的构建方式是使用 Cinema 4D SDK 的**顶层 CMake 工作流**，通过 `MAXON_SDK_CUSTOM_PATHS_FILE` 把本仓库作为外部模块接入。

仓库中提供了一个 Windows 辅助脚本：

- `scripts/Configure-HOT4D-C4D2026SDK.ps1`

典型用法：

```powershell
pwsh -File .\scripts\Configure-HOT4D-C4D2026SDK.ps1 `
  -SdkRoot 'C:\path\to\Cinema_4D_CPP_SDK_2026_1_0' `
  -Build
```

关于更详细的构建说明、注册链说明、移植经验，请看：

- `BUILDING.md`

## 说明

本仓库是一个适配 / 维护延续版本，**不是**原始上游仓库。

如果你想查看原始历史和上下文，请从这里开始：

https://github.com/Valkaari/HOT4D
