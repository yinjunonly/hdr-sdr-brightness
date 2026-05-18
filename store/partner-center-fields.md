# Partner Center 字段参考

填写 Partner Center 时，可以把这份文档当作提交清单。这里是中文说明版，产品名、URL、命令和文件路径需要按原文填写。

## 产品设置

```text
已保留的产品名称：HDR SDR Brightness Assistant
包标识名称：InjunaId.HDRSDRBrightnessAssistant
发布者：CN=81C6B903-8D8C-435A-B42E-3678C731A484
发布者显示名称：InjunaId
类别：Utilities & tools / 实用工具
定价：付费
基础价格：选择较低付费档位，目标是最接近 US$1.99 的档位
首发折扣：14 天 5 折，面向所有用户，目标是最接近 US$0.99 的折后档位
免费试用：限时试用，7 天，完整功能
市场：选择所有可用市场，包括中国；如果有“包括未来市场”选项，也勾选
隐私政策 URL：https://github.com/yinjunonly/hdr-sdr-brightness/blob/main/docs/privacy-policy.md
支持 URL：https://github.com/yinjunonly/hdr-sdr-brightness/blob/main/docs/support.md
网站 URL：可选
```

## 应用属性

建议回答：

```text
此应用是否访问互联网？核心功能不需要访问互联网，选 No / 否。
此应用是否需要登录？No / 否。
此应用是否包含应用内购买？No / 否。
此应用是否包含订阅？No / 否。
此应用是否提供免费试用？Yes / 是，使用 Microsoft Store 管理的 7 天限时试用。
此应用是否包含广告？No / 否。
此应用是否收集个人信息？No / 否。
此应用是否使用位置？No / 否。
此应用是否使用麦克风、摄像头、联系人或日历？No / 否。
此应用是否安装驱动或服务？No / 否。
此应用是否开机启动？可选，仅在用户主动启用“随 Windows 启动”后才会开机启动。
目标设备系列：Windows.Desktop
最低 OS：Windows 10 version 1809 / 10.0.17763.0
架构：x64
```

年龄分级参考：

```text
非游戏工具软件。
无暴力内容。
无性内容。
无赌博内容。
无用户生成内容。
无不受限制的网页访问。
无社交互动。
无位置共享。
应用本身不收集个人数据。
```

## 定价

建议的 Store 定价策略：

```text
一次性付费购买应用。
基础价格：选择较低付费档位，目标是最接近 US$1.99 的档位。
首发折扣：前 14 天 5 折，面向所有用户。
首发折扣目标：选择折后最接近 US$0.99 的档位。
中国市场：保留中国市场，除非 Partner Center 显示的换算价格明显不合理，否则使用 Store 推荐的本地价格。
市场：选择所有可用市场，包括中国；如果 Partner Center 提供“包括未来市场”选项，也保留勾选。
免费试用：限时 7 天，完整功能。
无应用内购买。
无订阅。
无捐赠提示。
Store 构建中不显示支持者码 UI。
```

提交时的选择：

```text
价格档位：低价档，目标是 US$1.99 等价档位。
发布市场：全球 / 所有可用市场，包括中国。
首发优惠：是，14 天 5 折，面向所有用户。
免费试用：是，7 天限时试用，完整功能。
```

理由：

```text
这是一个聚焦单一用途的工具软件，低价一次性购买能降低用户决策成本，也符合“低价、更高销量”的策略。
不建议首发免费，因为这会削弱付费应用定位，也可能吸引大量低意向安装。
7 天完整功能试用可以让用户在付费前确认 SDR 亮度调节是否适合自己的 HDR 显示器。
除非后续在应用内加入 Store 授权检查，否则不要使用无限期试用或功能受限试用。
```

年龄分级：

```text
按非游戏工具软件完成 Partner Center 年龄分级问卷。
预期结果：最低 / 全年龄段分级，因为应用没有暴力、性内容、赌博、用户生成内容、不受限制的网页访问、社交互动、位置共享，也不收集个人数据。
```

## 包上传

仅在明确准备 Store 包之后使用：

```powershell
powershell -ExecutionPolicy Bypass -File .\package-msix.ps1 -Version 1.0.6 -Clean
```

上传这个文件：

```text
dist\HdrSdrBrightness-1.0.6-win64.msixupload
```

不要上传：

```text
GitHub ZIP 发布包
未签名的独立 EXE
package.ps1 生成的桌面 ZIP 包
```

## 认证说明

粘贴或附加这个文件的内容：

```text
store\certification-notes.md
```
