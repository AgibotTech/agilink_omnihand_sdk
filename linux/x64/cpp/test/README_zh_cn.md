# AGILINK OmniHand SDK C++ 单元测试

**说明：** **OmniHand 2025 (O10)** 的工厂方法及广播发现接口在 C++ 头文件中使用 `std::uint8_t` 表示 `hand_device_id` 与 `canfd_*` 索引；测试代码使用整型字面量，依赖隐式转换。详见 `release/doc/zh_cn/API_CPP_O10.md`。

本目录包含基于 GoogleTest 的单元测试，涵盖所有 OmniHand 产品：
- **OmniHand 2025 (O10)** - 10 自由度，1D 触觉传感器
- **OmniHand Pro 2025 (O12)** - 12 自由度，3D 触觉传感器
- **OmniHand 3 Lite (H3L)** - 4 自由度，通过 OmniHand3LiteGesture 枚举进行手势控制

## 编译测试

测试默认禁用。要启用测试，使用以下 CMake 配置：

```bash
cmake -DBUILD_CPP_TESTS=ON ..
cmake --build .
```

或者从项目根目录构建：

```bash
cmake -DBUILD_CPP_TESTS=ON -B build
cmake --build build
```

内部协议和维护测试默认关闭。SDK 维护者可在源码构建时添加 `-DOMNIHAND_BUILD_INTERNAL_TESTS=ON`；这些测试需要内部头文件。

## 运行测试

编译后，使用 CTest 运行测试：

```bash
cd build
ctest
```

或者运行单个测试可执行文件：

```bash
# 运行 O10 测试（默认频率：10 Hz）
./cpp/test/test_omnihand_2025

# 运行 O10 测试，自定义频率（5-500 Hz）
./cpp/test/test_omnihand_2025 -f 20

# 运行 O12 测试（默认频率：10 Hz）
./cpp/test/test_omnihand_pro_2025

# 运行 O12 测试，自定义频率（5-500 Hz）
./cpp/test/test_omnihand_pro_2025 -f 33

# 运行 H3L 测试
./cpp/test/test_omnihand_3_lite

# H3UM：同时选择实物左右手、CAN 适配器索引及通道
./bin/omnihand/test/test_omnihand_3_ultra_m -d zlgcan -i 0 -c 1 --hand right
```

H3UM 默认为 `--hand left`，右手须指定 `--hand right`，也接受 `--hand-type`。
左右手决定预设动作和角度换算表；T1.3/T1.5 版本识别与左右手选择相互独立。

### 请求频率参数

O10 和 O12 测试程序支持 `-f FREQ` 参数来设置 CAN 请求频率：

- **范围**：5-500 Hz
- **默认值**：10 Hz
- **用法**：`./test_omnihand_2025 -f 20`（设置频率为 20 Hz）


## 测试结构

每个测试文件（`test_omnihand_*.cc`）包含：

- **工厂方法测试**：验证对象创建
- **初始化测试**：测试设备初始化（需要硬件）
- **设备信息测试**：测试设备 ID 和厂商信息
- **电机控制测试**：测试位置、速度和角度控制（需要硬件）
- **传感器测试**：测试触觉传感器和错误报告（需要硬件）
- **运动学测试**：测试正向运动学计算

## 硬件要求

大多数测试需要实际硬件连接：
- ZLG USB CANFD 适配器
- OmniHand 设备已连接并上电

不需要硬件的测试：
- 工厂方法创建
- 设备信息（无需 Init）
- 设备 ID 设置

## 注意事项

测试设计为优雅地处理缺失硬件的情况：
- 测试在执行硬件相关操作前检查 `Init()` 结果
- 如果硬件不可用，测试不会失败（将跳过硬件相关的断言）

对于没有硬件的 CI/CD 环境，您可以：
1. 模拟硬件接口
2. 跳过硬件相关测试
3. 使用模拟硬件响应的测试夹具

## 更多信息

- [English Documentation](README.md)
## 时间戳接口测试

OP3 gtest 支持 `-d zlgcan`、`-d hcan`、`-d socketcan`（Linux）和
`-d rs485`。RS485 波特率固定为 460800，必须指定串口：

```sh
test_omnipicker_3 -d rs485 -p /dev/ttyUSB0
test_omnipicker_3 -d rs485 /dev/ttyUSB0
test_omnipicker_3.exe -d rs485 -p COM7
```

时间戳测试放在 O10、O12、H3L、H3UM、OP3 各自原有测试程序中。
连接对应设备后，用过滤器只执行时间戳接口测试，例如：

```sh
test_omnihand_2025 -d hcan --gtest_filter="*WithTimestamp*"
```

日期日志统一显示 `2026-10-30 14:43:42.123456`，不重复打印 Unix 微秒整数。
各型号命令、无硬件日期格式测试和时间语义见 [C++ 反馈接收时间戳](../TIMESTAMPED_FEEDBACK.md#验证)。
