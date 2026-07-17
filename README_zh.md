# ARM CAN Tool

ARM CAN Tool 是面向 ARM 和 RISC-V 嵌入式系统的调试探针。集成 SWD/JTAG 调试接口与电气隔离 CAN 2.0A/B 接口。调试输出与 CAN 总线数据合并为单一 USB 串行日志流。内置 Lua 解释器，可通过脚本访问调试接口、CAN 总线和 SD 卡。

---

## 资源

| 资源 | 地址 |
|------|------|
| 固件源码 | [github.com/koendv/arm_can_tool](https://github.com/koendv/arm_can_tool) |
| 硬件设计（EasyEDA / OSHWLab） | [oshwlab.com/koendv/arm_can_tool](https://oshwlab.com/koendv/arm_can_tool) |

完整章节目录见 [README.md](README.md)（英文）。

---

## 简体中文菜单

OLED 菜单已提供简体中文界面，覆盖全部功能选项。

菜单源码：[`mui_form_zh.c`](applications/mui_form_zh.c)

---

## 关于翻译质量

本项目的简体中文翻译由一名非中文母语的开发者完成。翻译可能不自然，欢迎指正。

如发现翻译错误、表达生硬或不符合习惯用法，请按以下格式提交 Issue（英文或中文均可）：

> `页面名称 | 英文原文 | 当前中文 | 修改建议`

示例：
> `CANBUS | logging | 日志 | 日志记录`

---

技术支持请使用英文提交 Issue。
