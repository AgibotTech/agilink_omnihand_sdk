# hand_receive_data — UDP 接收 `hand_send_data` 并控制 OmniHand 2025

本目录提供示例程序，通过 **UDP + Protobuf**（与 `hand_send_data/test_ik_genie_msg_UDP.py` 一致）接收 **10 维主动关节角（弧度）**，并调用 SDK：

- `OmniHand2025.set_all_active_joint_angles(...)`

## 依赖

- 已按 `omnihand2025/python` 说明安装 **omnihand** Python 包（含本机 CAN 驱动）。
- `google.protobuf`（通常随环境已有；若无：`pip install protobuf`）。

## 运行前

1. 连接 OmniHand 2025（O10），确认 **ZLG CAN / HCAN** 设备可用。
2. 在发送端运行 `hand_send_data` 的 `test_ik_genie_msg_UDP.py`，默认向 **`127.0.0.1:5000`** 发 UDP。
3. 若接收端与发送端不在同一台机器，请将发送脚本里的 `UdpSender` 目标 IP 改为接收机 IP，本程序用 `--udp-host 0.0.0.0` 监听所有网卡。

## 命令示例

在 **`omnihand2025/python`** 目录下（保证能 `import omnihand`）：

```bash
cd /path/to/omnihand2025/python
PYTHONPATH=. python demo/lingchu/hand_receive_data/udp_receive_control.py
```

**仅一只灵巧手**（例如只有右手，CAN 通道 0）：

```bash
PYTHONPATH=. python demo/omnihand_2025/hand_receive_data/udp_receive_control.py \
  --mode right --single-channel 0 -d zlgcan
```

**双手**（左通道 0、右通道 1，与 `demo_set_motor_via_multichannels.py` 类似）：

```bash
PYTHONPATH=. python demo/omnihand_2025/hand_receive_data/udp_receive_control.py \
  --mode dual --left-channel 0 --right-channel 1 -d zlgcan
```

**不连硬件，只测解析**：

```bash
PYTHONPATH=. python demo/omnihand_2025/hand_receive_data/udp_receive_control.py --dry-run -v
```

## 协议说明（与发送端一致）

- 消息类型：`RetargetInfoArray`，字段 `retarget_infos` 可含多条。
- 每条 `RetargetInfo`：
  - `group_id`: `GROUP_LEFT_TOOL` / `GROUP_RIGHT_TOOL`
  - `control_type`: `ABS_JOINT`（绝对关节位置，弧度）
  - `target_joint_positions`: 长度应为 **10**（不足会补 0，超出会截断）

## 注意

- 若你使用**官方生成的** `genie_msgs_pb` 且与当前动态描述字段编号不一致，请替换本目录下 `genie_msgs_pb/msg/*_pb2.py` 为你们的生成文件，或调整发送端与本目录描述一致。
- 关节顺序须与 `hand_send_data` 中 IK 输出的 `active_names` 一致；若左右手镜像或顺序不同，需自行在接收端做重映射（本示例未做重映射）。
