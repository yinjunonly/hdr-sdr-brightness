# HDR SDR 亮度助手

[English README](README.md)

一个极简 Windows 托盘工具，用来在开启 HDR 时自动调整 **Windows SDR 内容亮度**。

适用于 OLED、QD-OLED、MiniLED、HDR 显示器、HDR 电视和支持 HDR 的笔记本屏幕，用来缓解开启 Windows HDR 后 SDR 应用过亮、过暗或发灰的问题。

## 默认行为

- 夜间 SDR 内容亮度：`10`
- 白天 SDR 内容亮度：`25`
- 切换方式：可用时跟随 Windows 夜间模式，否则使用默认时段
- 默认时段：`18:00` 进入夜间，`08:00` 进入白天
- 启动后立即应用当前时段的 SDR 内容亮度
- 每 15 秒检查一次手动修改，并恢复到配置值
- 恢复手动修改时显示系统通知
- 调高或调低都会平滑渐变
- 可以在设置中关闭“自动纠正手动调整”
- 点击恢复通知会打开详情弹框

## 构建

需要 Windows 上的 MinGW `g++` 和 `windres`。

项目版本号存放在 `VERSION`。当前版本是 `1.0.0`。

```powershell
.\build.ps1
```

输出文件：

```text
bin\HdrSdrBrightness.exe
```

构建产物会生成到 `bin\` 和 `obj\` 目录，这些目录不会提交到 Git。

生成 GitHub Release 用的压缩包：

```powershell
.\package.ps1
```

输出文件：

```text
dist\HdrSdrBrightness-1.0.0-win64.zip
```

压缩包只包含可执行文件、许可证和说明文档。

## 使用

运行 `bin\HdrSdrBrightness.exe` 会打开设置页，同时程序也会留在系统托盘。

如需安静地只进托盘，可使用 `bin\HdrSdrBrightness.exe --background`；开机自启会使用这个模式。

托盘菜单：

- 立即应用
- 设置：语言、SDR 内容亮度、默认时段、开机自启
- 开机自启
- 打开显示设置
- 退出

## 界面与图标

- 支持中文和英文界面，默认自动跟随系统语言
- 浅色和深色模式跟随 Windows 应用主题
- 设置页使用自绘的 Windows 设置风格界面：卡片、开关、滑杆、分段控件和悬停效果
- 圆角控件使用 GDI+ 抗锯齿绘制
- 托盘悬停会显示当前模式、目标 SDR 内容亮度、HDR 显示器状态和自动纠正状态
- 托盘图标已改为适合小尺寸识别的高对比抽象图形

## 说明

程序使用当前用户注册表保存配置：

```text
HKCU\Software\OledHdrSdrSync
```

为了兼容旧版本，配置路径暂时保留旧名称。

开机自启使用：

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run\HdrSdrBrightness
```

旧的 `HdrSdrSync` / `OledHdrSdrSync` 开机自启项会在保存设置时自动迁移。

不需要管理员权限。

## 开源许可

MIT License，详见 [LICENSE](LICENSE)。

## 搜索关键词

很多用户不会直接搜索软件名，而是会用下面这些问题描述搜索，所以 README 保留这些关键词，方便同类用户发现项目：

```text
Windows HDR 自动亮度
Windows HDR SDR 亮度
Windows SDR 内容亮度
Windows SDR 内容亮度滑块
HDR 下 SDR 内容过亮
HDR 下 SDR 内容过暗
HDR 开启后桌面太亮
HDR 开启后桌面太暗
HDR 开启后颜色发灰
HDR 发灰
HDR 洗白
Auto HDR 发灰
OLED HDR 太亮
OLED HDR 太暗
QD-OLED HDR 亮度
MiniLED HDR 太亮
MiniLED HDR 太暗
HDR 显示器 SDR 亮度
HDR 电视 Windows SDR 亮度
Windows 夜间模式 SDR 亮度
SDR White Level
```
