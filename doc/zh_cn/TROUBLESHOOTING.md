# 故障排除指南

[English](../en/TROUBLESHOOTING.md)

## 常见问题

### 1. 设备未找到 / 连接失败

**症状：**
- `Failed to initialize`
- `CANFD device not found`
- `USB device not found`

**解决方案：**

#### 检查适配器是否被系统识别

```bash
# Linux: 检查设备是否被识别
lsusb | grep -i zlg
# 预期输出: Bus XXX Device YYY: ID 3068:0009 ZLG USBCANFD-200U

# 检查串口
ls -la /dev/ttyACM* /dev/ttyUSB*
```

#### 配置 USB 权限（Linux）
```bash
# 运行配置脚本
sudo ./setup_udev.sh

# 注销并重新登录（必须！）

# 验证权限
ls -la /dev/ttyACM0  # 应显示 rw-rw-rw-
```

#### 检查设备索引
如果有多个 CANFD 适配器：
```python
# 尝试不同的设备索引
hand = OmniHand2025.create_hand_by_zlgcan(HandType.LEFT, 1, 0, 0)  # 第一个适配器
hand = OmniHand2025.create_hand_by_zlgcan(HandType.LEFT, 1, 1, 0)  # 第二个适配器
```

---

### 2. 权限被拒绝

**症状：**
- `Permission denied: /dev/ttyACM0`
- `LIBUSB_ERROR_ACCESS`
- 需要使用 `sudo` 运行

**解决方案：**

#### Linux
```bash
# 运行 udev 配置
sudo ./setup_udev.sh

# 手动添加用户到组
sudo usermod -aG dialout $USER
sudo usermod -aG plugdev $USER

# 注销并重新登录

# 验证组成员资格
groups  # 应包含 'dialout' 和 'plugdev'
```

#### Windows
- 右键点击"以管理员身份运行"
- 如未安装，请安装 ZLG USB 驱动程序

---

### 3. 通信超时

**症状：**
- `Request timeout`
- `No response from device`
- 间歇性失败

**解决方案：**

#### 检查 CANFD 参数

以下以 O10 为例。先初始化适配器，再通过广播查询设备地址；查询成功后，对象会缓存查询到的协议地址。

```python
from omnihand import HandType, OmniHand2025

hand = OmniHand2025.create_hand_by_zlgcan(
    hand_type=HandType.LEFT, hand_device_id=1,
    canfd_device_id=0, canfd_channel_id=0,
)

# 按适配器序列号选择时，用以下调用替换上面的创建语句。
# hand = OmniHand2025.create_hand_by_zlgcan(
#     hand_type=HandType.LEFT, hand_device_id=1,
#     usbcanfd_serial_number="YOUR_DEVICE_SN", canfd_channel_id=0,
# )

if hand is None or not hand.init():
    raise RuntimeError("无法初始化 O10 适配器")

discovery_result = hand.get_hand_device_id_by_broadcast()
if discovery_result < 0:
    raise RuntimeError("设备地址查询失败或协议地址不一致")
print(f"O10 地址查询结果: {discovery_result}")
```

`0` 表示 O10 的默认协议地址组合；正数表示自定义地址。后续通信使用对象缓存的地址，无需重新创建对象。

> **关于 `canfd_channel_id` 说明**：
> - 双通道适配器（USBCANFD-200U）：`can0` → `channel_id=0`，`can1` → `channel_id=1`
> - 单通道适配器（USBCANFD-100U）：始终使用 `channel_id=0`

#### 检查电源与 CAN 接线

- 确保电源稳定：H3UM 使用 48V，其他产品使用 24V。
- 检查电源连接是否松动。
- 确认灵巧手上的 LED 亮起。
- 如果 LED 已亮但 CAN 通信仍无响应，先断电，尝试交换 CAN_H 与 CAN_L，再重新上电测试通信。

#### 降低通信频率
```python
# 增加请求间隔
hand.set_request_interval(100)  # 请求间隔 100ms

# 增加响应超时阈值
hand.set_frame_recv_timeout(500)  # 每次请求超时 500ms
```

---

### 4. 电机不动

**症状：**
- 命令已发送但无移动
- 位置读数不变

**解决方案：**

#### 检查控制模式

控制模式接口因产品而异。O10 没有绑定 `set_all_control_modes()`；O12、H3UM 接收逐关节模式数组，支持的模式见各产品 API。

以下仅针对已初始化的 O12 对象 `o12_hand`，将各关节设为位置模式：

```python
from omnihand import ControlMode

modes = [int(ControlMode.POSITION)] * o12_hand.get_doa()
o12_hand.set_all_control_modes(modes)
```

参见 [O12 Python API](API_PYTHON_O12.md)、[H3UM Python API](API_PYTHON_H3UM.md)。

#### 检查位置反馈与目标数据

先读取已初始化对象 `hand` 的当前位置，并确认反馈数量与主动关节数一致：

```python
positions = hand.get_all_joint_positions()
if len(positions) != hand.get_doa():
    raise RuntimeError("位置反馈数量与主动关节数不一致")
print(positions)
```

电机编码控制使用 `set_all_joint_positions()`，角度控制使用 `set_all_active_joint_angles()`。目标数据须匹配产品、左右手和硬件版本，并符合该产品的关节数量与机械约束。`0`、`4096` 或全零 rad 均不能视为所有产品通用的安全位置；请参照对应产品 API 及预设手势。

#### 检查错误状态
```python
# 读取错误报告
errors = hand.get_all_error_reports()
for i, err in enumerate(errors, start=1):
    flags = {
        name: getattr(err, name)
        for name in (
            "stalled", "overheat", "over_current", "motor_except", "commu_except"
        )
    }
    if any(flags.values()):
        print(f"关节 {i} 错误: {flags}")
```

---

### 5. 导入错误（Python）

**症状：**
- `ModuleNotFoundError: No module named 'omnihand'`
- `ImportError: DLL load failed`

**解决方案：**

#### 重新安装包

在发布包的平台目录（`linux/x64`、`linux/aarch64` 或 `windows/x64`）运行，使用与应用程序相同的 Python 解释器。pip 会从本地 `python/` 目录选择兼容的 wheel，无需用通配符指定全部文件。

**Linux：**

```bash
python3 --version
python3 -m pip install --force-reinstall --no-deps --no-index --find-links=python omnihand
```

**Windows：**

```powershell
python --version
python -m pip install --force-reinstall --no-deps --no-index --find-links=python omnihand
```

当前发布包提供 CPython 3.10–3.14 的 wheel，请同时确认 Python 版本、操作系统和架构匹配。

#### 安装测试依赖（运行测试时需要）
```bash
pip install pytest numpy
```

#### Windows DLL 问题
- 安装 Visual C++ Redistributable 2019+
- 确保 `zlgcan.dll` 可访问

---

### 6. CMake 找不到包（C++）

**症状：**
- `Could not find package omnihand`
- `omnihand_DIR not set`

**解决方案：**

```cmake
# 设置正确的路径
set(OMNIHAND_ROOT "/usr/local")  # Linux 默认
# set(OMNIHAND_ROOT "C:/omnihand")  # Windows（默认安装目录，见 install.bat）

list(APPEND CMAKE_MODULE_PATH "${OMNIHAND_ROOT}/share/cmake/omnihand")
find_package(omnihand REQUIRED)
```

---

### 7. 命名空间冲突（C++）

**症状：**
- 编译错误：`'HandType' is ambiguous`（'HandType' 不明确）
- `'OmniHand2025' is ambiguous`（'OmniHand2025' 不明确）
- 与其他库发生冲突

**解决方案：**

#### 使用完整命名空间（推荐）

避免使用 `using namespace` 以防止命名冲突：

```cpp
#include "omnihand/omnihand_2025.h"
// 避免：using namespace agilink::omnihand;

int main() {
    // 使用完整命名空间
    auto hand = agilink::omnihand::OmniHand2025::createHandByZlgcan(
        agilink::omnihand::HandType::LEFT, 1, 0, 0
    );
    
    if (!hand || !hand->Init()) {
        return -1;
    }
    
    return 0;
}
```

#### 或使用类型别名（如需要）

如果需要更短的名称，可以使用类型别名：

```cpp
#include "omnihand/omnihand_2025.h"

namespace oh = agilink::omnihand;  // 类型别名

int main() {
    auto hand = oh::OmniHand2025::createHandByZlgcan(
        oh::HandType::LEFT, 1, 0, 0
    );
    // ...
}
```

**为什么避免使用 `using namespace`？**
- 防止与其他库发生冲突（如 ROS2、Eigen）
- 使代码更明确、更易理解
- 更适合具有多个依赖项的大型项目

---

### 8. ROS2 节点无法启动（Linux）

**症状：**
- `Package 'omnihand_node' not found`
- 节点无法通信

**解决方案：**

```bash
# 自动加载匹配的 ROS2 环境与 OmniHand 包
source ros2/setup.bash

# 验证包
ros2 pkg list | grep omnihand

# 检查节点
ros2 run omnihand_node omnihand_2025_node
```

---

## 诊断命令

### Linux

```bash
# 检查 USB 设备
lsusb -v | grep -A 20 "ZLG\|3068"

# 检查串口
dmesg | tail -20

# 检查 udev 规则
cat /etc/udev/rules.d/99-omnihand-usb.rules

# 检查用户组
groups $USER

# 检查 SDK 是否可导入（不连接设备）
python3 -c "from omnihand import OmniHand2025; print('OK')"
```

### Windows

```cmd
# 在设备管理器中检查 USB
devmgmt.msc

# 检查 Python
python --version
pip show omnihand
```

---

## 获取帮助

如果问题仍然存在：

1. **收集信息：**
   - 操作系统版本
   - SDK 版本（查看 `VERSION` 文件）
   - 错误消息（完整输出）
   - 硬件型号（O10/O12，适配器类型）

2. **检查日志：**
   对已初始化的 `hand` 对象开启 C++ SDK 收发明细：

   ```python
   hand.show_data_details(True)
   ```

   日志默认输出到标准输出，请保留完整的发送、接收与错误信息。

3. **联系支持：**
   - 邮箱：xuqigui@agibot.com
   - 请附上收集到的信息
