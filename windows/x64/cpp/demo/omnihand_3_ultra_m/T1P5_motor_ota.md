# T1.5 关节电机固件升级

使用现有 `OmniHand3UltraM` 对象调用同步 C++ 接口。全局关节号为 1～21，
固件文件为原始 `.bin`，路径使用 UTF-8。目前支持 T1.5 CAN 连接。

透传完全由 SDK 内部管理，不暴露开关 API。每次扫描或单个电机升级前开启透传，
结束时关闭；失败或回调抛出异常也会尝试关闭。开启命令发送失败时不发送电机指令，
仍尝试发送退出命令。扫描和升级共用互斥锁，保证整个操作及透传收尾串行执行。
调用期间暂停灵巧手的普通控制与轮询；Toolbox 的电机页与手固件页保持隔离，
进入、返回或空闲停留在电机页均不改变透传状态。

扫描接口：

```cpp
bool ScanJointMotorFirmware(
    std::function<void(uint8_t node_id, bool in_boot,
                       const std::string& version)> callback);

// Pause normal hand requests while the synchronous scan runs.
const bool scanned = hand->ScanJointMotorFirmware(
    [](uint8_t node_id, bool in_boot, const std::string& version) {
      printf("Node %u: %s %s\n", static_cast<unsigned>(node_id),
             in_boot ? "BOOT" : "APP", version.c_str());
    });
```

扫描全局节点 1～21，每个节点先查 Boot 版本，无有效回包再查 CANopen。
只有识别为 BOOT 或 APP 的节点才回调，不改变电机模式。`in_boot=true` 表示 BOOT，
`false` 表示 APP。回调在扫描调用线程同步执行，需自行复制要保留的版本字符串；
回调内不要再次调用手对象的方法，抛出的异常会在尝试退出透传后继续抛给调用方。
返回 `true` 表示扫描完成，即使没有发现节点也返回 `true`；无效回调、不支持的设备、
开启／退出透传失败或通信异常返回 `false`。该接口不提供 Python 绑定。

```cpp
bool UpdateJointMotorFirmware(
    uint8_t node_id, const std::string& file_name,
    OtaProgressCallback progress_callback = nullptr,
    int frame_interval_us = 2000);
```

```cpp
using namespace agilink::omnihand;

// The SDK enters and exits pass-through for this call.
const bool success = hand->UpdateJointMotorFirmware(
    9, "motor_firmware.bin",
    [](int current, int total, OtaProgressStatus status) {
      if (status == OtaProgressStatus::AGILINK_OTA_TRANSMITTING) {
        printf("CRC confirmed: %d/%d blocks\n", current, total);
      } else if (status == OtaProgressStatus::AGILINK_OTA_ERROR) {
        printf("Motor upgrade failed: SDK error %d\n", current);
      }
    });
```

进度回调复用灵巧手 OTA 的 `OtaProgressCallback`，块大小为 1024 字节，
完成块数仅在 CRC／写入应答成功后递增。回调内不要再次调用手对象的方法。
此升级接口不提供 Python 绑定，意优协议类型与探测结果保留在 `impl` 内部。

内部流程：

1. 完整读取并校验文件，开启透传后才发送电机指令。
2. 先查询 Boot 协议版本，依据 TYPE 判断 Boot／App；未收到有效回包时读
   CANopen `0x100A/0`。CANopen 成功即为 App。
3. App 只通过 CANopen `0x2109/0 = 0x12345678` 进入 Boot，等待 500 ms 后
   复查 TYPE。已在 Boot 的关节直接继续。
4. 擦除长度向上对齐到 1K，等待擦除完成回包，超时为 5 s。
5. 按文件偏移 `0、1024、2048…` 准备地址并检查缓冲区，逐块发送数据。
   首帧携带 3 字节，续帧携带 6 字节，续帧序号从 `0x21` 开始，
   经 `0x2F` 到 `0x20` 循环，首帧不占序号。尾块使用实际长度，填充为 `0xFF`。
   CRC16/XMODEM 只计算实际固件数据。准备地址和 CRC／写入的超时均为 3 s。
6. 依据更新版参考程序 `idriver.cpp`，三类操作回包均为
   `ID=0xFF, DATA=FE 06 F4 STATUS ...`；STATUS 为 0 才成功。
7. 跳回 App，等待 500 ms 并重新查询模式，随后关闭透传。
   全部完成且退出命令发送成功，才通知升级成功并返回 `true`。

使用现有主机到手的 CAN FD 通道发送原始 11 位 CAN ID 和 8 字节意优数据，
手固件负责内部总线映射与电机 CAN 转发。SDK 不提前折算全局关节号。

`frame_interval_us` 为续帧发送后的间隔，可设置为 0～1,000,000 微秒，默认
2000 微秒。该默认值仍需结合实机调整。升级期间临时取消普通请求的毫秒限速，
本次升级返回或抛异常时恢复原设置。

每次扫描及单个电机升级各自拥有一对透传开启／退出命令；多选升级时逐个调用即可。
普通升级失败返回 `false`，日志记录失败阶段及电机原始状态码。
回调错误沿用 SDK 错误码：`-1` 不支持或参数无效、`-2` 打开文件失败、
`-3` 文件长度或读取无效、`-4` 探测或进入 Boot 失败、`-5` 未确认回到 App、
`-6` 擦除／地址／缓冲区失败、`-7` CRC／写入失败、`-8` 通信／透传失败。

与参考程序一致，不自动重试写入块。升级中断后若停留在 Boot，重新调用即可
从偏移 0 擦除并重刷。末尾只确认回到 App，原始 bin 没有版本元数据供 SDK
判断是否为某个指定的新版本。透传开关没有定义确认回包，关闭成功表示命令已发送。
退出发送失败时接口返回失败；物理断线或进程被强制结束时，软件无法保证退出命令送达。

三类 `F4` 回包没有节点号或操作号，SDK 串行等待并丢弃等待前的回包；协议本身
无法完全区分当前操作的应答与延迟到达的同类重复应答。
