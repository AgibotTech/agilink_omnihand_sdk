# Python Examples

**API:** O10 examples pass `hand_device_id` / `canfd_*` as integers; these match the Python binding (native side uses `uint8_t`). See `release/doc/en/API_PYTHON_O10.md`.

## OmniHand 2025 (10 DOF) Examples
- `omnihand_2025/demo_get_hardware_info.py` - Get hardware information
- `omnihand_2025/demo_monitor_current.py` - Monitor motor current
- `omnihand_2025/demo_monitor_error.py` - Monitor errors
- `omnihand_2025/demo_monitor_temperature.py` - Monitor temperature
- `omnihand_2025/demo_set_angle.py` - Set finger angles
- `omnihand_2025/demo_set_motion.py` - Set motion parameters
- `omnihand_2025/demo_set_motor.py` - Direct motor control
- `omnihand_2025/demo_set_motor_via_multicans.py` - Control via multiple CANFD adapters
- `omnihand_2025/demo_set_motor_via_multichannels.py` - Control via multiple CAN channels
- `omnihand_2025/demo_set_motor_via_multisocketcans.py` - Control via multiple SocketCAN interfaces
- `omnihand_2025/demo_set_torque.py` - Set torque
- `omnihand_2025/demo_tactile_sensor.py` - Read tactile sensors
- `omnihand_2025/demo_tactile_sensor_raw.py` - Read tactile sensor raw data
- `omnihand_2025/demo_tactile_sensor_raw_reliability.py` - Tactile sensor reliability test
- `omnihand_2025/demo_all_tactile_sensor_raw_reliability.py` - All tactile sensors reliability test
- `omnihand_2025/demo_set_angle_and_tactile_sensor_reliability.py` - Angle and tactile sensor reliability test
- `omnihand_2025/demo_100set_and_20tactile_reliability.py` - High-frequency control and sensor test
- `omnihand_2025/demo_canfd_id.py` - CANFD communication example (by device ID)
- `omnihand_2025/demo_canfd_serial.py` - CANFD communication example (by serial number)
- `omnihand_2025/demo_socketcan.py` - SocketCAN example (Linux only)

## OmniHand Pro 2025 (12 DOF) Examples
- `omnihand_pro_2025/demo_get_hardware_info.py` - Get hardware information
- `omnihand_pro_2025/demo_monitor_current.py` - Monitor motor current
- `omnihand_pro_2025/demo_monitor_error.py` - Monitor errors
- `omnihand_pro_2025/demo_monitor_temperature.py` - Monitor temperature
- `omnihand_pro_2025/demo_sensor_touch.py` - Read touch sensors
- `omnihand_pro_2025/demo_set_angle.py` - Set finger angles
- `omnihand_pro_2025/demo_set_position.py` - Set finger positions
- `omnihand_pro_2025/demo_set_velocity.py` - Set finger velocity
- `omnihand_pro_2025/demo_canfd_id.py` - CANFD communication example (by device ID)
- `omnihand_pro_2025/demo_canfd_serial.py` - CANFD communication example (by serial number)
- `omnihand_pro_2025/demo_socketcan.py` - SocketCAN example (Linux only)

## OmniHand 3 Lite (4 DOF) Examples
- `omnihand_3_lite/demo_get_hardware_info.py` - Get hardware information
- `omnihand_3_lite/demo_set_motor.py` - Motor position control and gesture control
- `omnihand_3_lite/demo_set_motor_via_multicans.py` - Control via multiple CANFD adapters
- `omnihand_3_lite/demo_set_get_position_reliability.py` - Position control reliability test
- `omnihand_3_lite/demo_set_get_reliability_csv.py` - Reliability test (CSV output)
- `omnihand_3_lite/demo_canfd_id.py` - CANFD communication example (by device ID)
- `omnihand_3_lite/demo_canfd_serial.py` - CANFD communication example (by serial number)
- `omnihand_3_lite/demo_socketcan.py` - SocketCAN example (Linux only)

## OmniHand 3 Ultra M (T1.3/T1.5) Example

`omnihand_3_ultra_m/demo_omnihand_3_ultra_m.py` uses the unified factories to detect
T1.3/T1.5, moves the pinky PIP through two angles, and reads motor feedback and raw
tactile data. T1.3 returns XYZ points; T1.5 reads pressure through a whole-hand query.
Use a matching rebuilt Python wheel.

```bash
python omnihand_3_ultra_m/demo_omnihand_3_ultra_m.py -d zlgcan -i 0 -c 0
python omnihand_3_ultra_m/demo_omnihand_3_ultra_m.py -d hcan -i 0 -c 1 --hand right
python omnihand_3_ultra_m/demo_omnihand_3_ultra_m.py -d socketcan --can-if can0
```

Device ID defaults to `0` for address discovery on the selected channel. Use
`--device-id ID` for a known address; `-i` selects the CAN adapter and `-c` its channel.

## Usage

```bash
# Make sure omnihand package is installed
pip list | findstr omnihand

# Run examples
python omnihand_2025\demo_get_hardware_info.py
python omnihand_pro_2025\demo_get_hardware_info.py
python omnihand_3_lite\demo_get_hardware_info.py
```
