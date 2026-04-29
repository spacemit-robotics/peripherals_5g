# 5G 组件（MR880A）

## 项目简介

5G 组件提供 MR880A 模组的 AT 指令封装与拨号能力，支持 ECM/NCM 网络模式下的初始化、SIM/注册信息获取、PDP 配置与数据连接，并在拨号成功后自动执行 DHCP 获取 IP。

## 功能特性

**支持：**
- UART AT 通信（自动查找 AT 串口）
- 设备基础信息、SIM/注册/信号信息查询
- PDP 上下文配置与 NDIS 拨号
- 自动识别 NCM 网口并执行 `udhcpc -i <ifname>`
- 内核配置检查（ECM/NCM）

**不支持：**
- RNDIS 模式

## 快速开始

### 环境准备

- CMake >= 3.10
- C99 编译器（GCC/Clang）
- 目标设备具备 MR880A 模组与 USB 连接
- 系统已安装 `udhcpc`

### 内核配置

初始化阶段会通过 `zcat /proc/config.gz` 检查内核配置：

**ECM 模式**
- `CONFIG_NETDEVICES=y`
- `CONFIG_USB_NET_DRIVERS=m`
- `CONFIG_USB_USBNET=m`
- `CONFIG_USB_NET_CDCETHER=m`

**NCM 模式**
- `CONFIG_USB_SERIAL=m`
- `CONFIG_USB_SERIAL_OPTION=m`
- `CONFIG_USB_USBNET=m`
- `CONFIG_USB_NET_CDCETHER=m`
- `CONFIG_USB_NET_CDC_NCM=m`

### 构建编译

脱离 SDK 单独构建：

```bash
cd components/peripherals/5g
mkdir -p build && cd build
cmake -DBUILD_TESTS=ON ..
make -j
```

> 注意：CMake 配置阶段会检查 `udhcpc` 是否已安装，否则直接报错。

### 运行示例

构建时启用 `BUILD_TESTS=ON` 后会生成测试程序 `test_5g_mr880a`：

```bash
# 自动识别 AT 口，设置 APN，启动拨号，自动 DHCP，Ping 验证(ping www.baidu.com)
./test_5g_mr880a -d auto -a ctnet

# 指定 AT 串口与波特率
./test_5g_mr880a -d /dev/ttyUSB0 -b 9600 -a ctnet
```

### 关键代码示例

```c
#include "5g.h"

struct modem_5g_dev *dev = modem_5g_alloc_uart("MR880A:mr880a0", "auto", 9600);
if (!dev)
    return -1;

if (modem_5g_init(dev) != MODEM_5G_STATUS_SUCCESS)
    return -1;

struct modem_5g_pdp_context ctx = {0};
ctx.cid = 1;
ctx.pdp_type = MODEM_5G_PDP_IPV4V6;
strncpy(ctx.apn, "ctnet", sizeof(ctx.apn) - 1);
modem_5g_set_pdp_context(dev, &ctx);

modem_5g_data_start(dev, 1);
modem_5g_free(dev);
```

## 详细使用

参考test/test_5g_mr880a.c。或者自行获取官方的AT指令说明文档。

## 常见问题

**Q: 自动识别不到 AT 串口？**
- 确认 USB 接口枚举完成，并存在 `bInterfaceProtocol=12` 的接口目录。
- 也可使用 `-d /dev/ttyUSB0` 手动指定。

**Q: 自动识别不到网口？**
- 使用 `cat /sys/class/net/*/device/interface` 查看是否存在 `NCM Network Control Model`。
- 若不是 NCM，确认模组 `AT+UNETMODECFG` 配置并重启生效。

**Q: DHCP 获取失败？**
- 确认 `udhcpc` 已安装且可执行。
- 确认网口处于 UP 状态，必要时手动 `ip link set <ifname> up`。

## 版本与发布

| 版本   | 日期       | 说明 |
| ------ | ---------- | ---- |
| 1.0.0  | 2026-02-28 | 初始版本，支持 MR880A UART/NCM 拨号。 |

## 贡献方式

欢迎参与贡献：提交 Issue 反馈问题，或通过 Pull Request 提交代码。

- **编码规范**：本组件 C 代码遵循 [Google C++ 风格指南](https://google.github.io/styleguide/cppguide.html)（C 相关部分），请按该规范编写与修改代码。
- **提交前检查**：请在提交前运行本仓库的 lint 脚本，确保通过风格检查：
  ```bash
  # 在仓库根目录执行（检查全仓库）
  bash scripts/lint/lint_cpp.sh

  # 仅检查本组件
  bash scripts/lint/lint_cpp.sh components/peripherals/5g
  ```
  脚本路径：`scripts/lint/lint_cpp.sh`。若未安装 `cpplint`，可先执行：`pip install cpplint` 或 `pipx install cpplint`。
- **提交说明**：提交 Issue 或 PR 前请描述模组型号、连接方式与复现步骤。

## License

本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
