# Troubleshooting Guide

[中文](../zh_cn/TROUBLESHOOTING.md)

## Common Issues

### 1. Device Not Found / Connection Failed

**Symptoms:**
- `Failed to initialize`
- `CANFD device not found`
- `USB device not found`

**Solutions:**

#### Check Adapter Detection

```bash
# Linux: Check if device is recognized
lsusb | grep -i zlg
# Expected output: Bus XXX Device YYY: ID 3068:0009 ZLG USBCANFD-200U

# Check serial ports
ls -la /dev/ttyACM* /dev/ttyUSB*
```

#### Configure USB Permissions (Linux)
```bash
# Run setup script
sudo ./setup_udev.sh

# Log out and log back in (required!)

# Verify permissions
ls -la /dev/ttyACM0  # Should show rw-rw-rw-
```

#### Check Device Index
If you have multiple CANFD adapters:
```python
# Try different device indices
hand = OmniHand2025.create_hand_by_zlgcan(HandType.LEFT, 1, 0, 0)  # First adapter
hand = OmniHand2025.create_hand_by_zlgcan(HandType.LEFT, 1, 1, 0)  # Second adapter
```

---

### 2. Permission Denied

**Symptoms:**
- `Permission denied: /dev/ttyACM0`
- `LIBUSB_ERROR_ACCESS`
- Need to run with `sudo`

**Solutions:**

#### Linux
```bash
# Run udev setup
sudo ./setup_udev.sh

# Manually add user to groups
sudo usermod -aG dialout $USER
sudo usermod -aG plugdev $USER

# Log out and log back in

# Verify group membership
groups  # Should include 'dialout' and 'plugdev'
```

#### Windows
- Right-click and "Run as Administrator"
- Install ZLG USB driver if not installed

---

### 3. Communication Timeout

**Symptoms:**
- `Request timeout`
- `No response from device`
- Intermittent failures

**Solutions:**

#### Check CANFD Parameters

The following example is for O10. Initialize the adapter, then discover the device address by broadcast. Successful discovery updates the object's cached protocol addresses.

```python
from omnihand import HandType, OmniHand2025

hand = OmniHand2025.create_hand_by_zlgcan(
    hand_type=HandType.LEFT, hand_device_id=1,
    canfd_device_id=0, canfd_channel_id=0,
)

# To select by adapter serial number, replace the factory call above with:
# hand = OmniHand2025.create_hand_by_zlgcan(
#     hand_type=HandType.LEFT, hand_device_id=1,
#     usbcanfd_serial_number="YOUR_DEVICE_SN", canfd_channel_id=0,
# )

if hand is None or not hand.init():
    raise RuntimeError("Failed to initialize the O10 adapter")

discovery_result = hand.get_hand_device_id_by_broadcast()
if discovery_result < 0:
    raise RuntimeError("Address discovery failed or protocol addresses differ")
print(f"O10 address discovery result: {discovery_result}")
```

`0` represents O10's default protocol address combination; a positive value represents a custom address. Subsequent requests use the cached addresses, so there is no need to recreate the object.

> **Note on `canfd_channel_id`**:
> - For dual-channel adapters (USBCANFD-200U): `can0` → `channel_id=0`, `can1` → `channel_id=1`
> - For single-channel adapters (USBCANFD-100U): always use `channel_id=0`

#### Check Power Supply and CAN Wiring

- Ensure the power supply is stable: 48V for H3UM, 24V for other products.
- Check for loose power connections.
- Confirm that the hand's LED is lit.
- If the LED is lit but CAN communication still gets no response, power off, try swapping CAN_H and CAN_L, then power on and test again.

#### Reduce Communication Rate
```python
# Increase request interval
hand.set_request_interval(100)  # 100ms between requests

# Increase response timeout threshold
hand.set_frame_recv_timeout(500)  # 500ms timeout for each request
```

---

### 4. Motor Not Moving

**Symptoms:**
- Commands sent but no movement
- Position readings don't change

**Solutions:**

#### Check Control Mode

Control mode APIs vary by product. O10 does not expose `set_all_control_modes()`. O12 and H3UM accept an array of per-joint modes; supported modes are listed in each product's API.

For an initialized O12 instance named `o12_hand`, set each joint to position mode as follows:

```python
from omnihand import ControlMode

modes = [int(ControlMode.POSITION)] * o12_hand.get_doa()
o12_hand.set_all_control_modes(modes)
```

See [O12 Python API](API_PYTHON_O12.md) and [H3UM Python API](API_PYTHON_H3UM.md).

#### Check Position Feedback and Target Data

Read the current positions from the initialized `hand` instance and check that the feedback count matches the active joint count:

```python
positions = hand.get_all_joint_positions()
if len(positions) != hand.get_doa():
    raise RuntimeError("Position feedback count does not match active DOF")
print(positions)
```

Use `set_all_joint_positions()` for motor encoder commands and `set_all_active_joint_angles()` for angle commands. Targets must match the product, hand side, hardware revision, joint count, and mechanical constraints. Neither `0`, `4096`, nor all-zero radians is a universally safe position; refer to the product's API and preset gestures.

#### Check Error Status
```python
# Read error reports
errors = hand.get_all_error_reports()
for i, err in enumerate(errors, start=1):
    flags = {
        name: getattr(err, name)
        for name in (
            "stalled", "overheat", "over_current", "motor_except", "commu_except"
        )
    }
    if any(flags.values()):
        print(f"Joint {i} error: {flags}")
```

---

### 5. Import Error (Python)

**Symptoms:**
- `ModuleNotFoundError: No module named 'omnihand'`
- `ImportError: DLL load failed`

**Solutions:**

#### Reinstall Package

Run from the release package's platform directory (`linux/x64`, `linux/aarch64`, or `windows/x64`) using the same Python interpreter as your application. pip selects a compatible wheel from the local `python/` directory; do not pass all wheels through a wildcard.

**Linux:**

```bash
python3 --version
python3 -m pip install --force-reinstall --no-deps --no-index --find-links=python omnihand
```

**Windows:**

```powershell
python --version
python -m pip install --force-reinstall --no-deps --no-index --find-links=python omnihand
```

The current package provides CPython 3.10–3.14 wheels. Check that the Python version, operating system, and architecture match.

#### Install Test Dependencies (for running tests)
```bash
pip install pytest numpy
```

#### Windows DLL Issues
- Install Visual C++ Redistributable 2019+
- Ensure `zlgcan.dll` is accessible

---

### 6. CMake Find Package Failed (C++)

**Symptoms:**
- `Could not find package omnihand`
- `omnihand_DIR not set`

**Solutions:**

```cmake
# Set the correct path
set(OMNIHAND_ROOT "/usr/local")  # Linux default
# set(OMNIHAND_ROOT "C:/omnihand")  # Windows (default install dir, see install.bat)

list(APPEND CMAKE_MODULE_PATH "${OMNIHAND_ROOT}/share/cmake/omnihand")
find_package(omnihand REQUIRED)
```

---

### 7. Namespace Conflicts (C++)

**Symptoms:**
- Compilation errors: `'HandType' is ambiguous`
- `'OmniHand2025' is ambiguous`
- Conflicts with other libraries

**Solutions:**

#### Use Fully Qualified Names (Recommended)

Avoid `using namespace` to prevent naming conflicts:

```cpp
#include "omnihand/omnihand_2025.h"
// Avoid: using namespace agilink::omnihand;

int main() {
    // Use fully qualified names
    auto hand = agilink::omnihand::OmniHand2025::createHandByZlgcan(
        agilink::omnihand::HandType::LEFT, 1, 0, 0
    );
    
    if (!hand || !hand->Init()) {
        return -1;
    }
    
    return 0;
}
```

#### Or Use Type Aliases (If Needed)

If you need shorter names, use type aliases instead:

```cpp
#include "omnihand/omnihand_2025.h"

namespace oh = agilink::omnihand;  // Type alias

int main() {
    auto hand = oh::OmniHand2025::createHandByZlgcan(
        oh::HandType::LEFT, 1, 0, 0
    );
    // ...
}
```

**Why avoid `using namespace`?**
- Prevents conflicts with other libraries (e.g., ROS2, Eigen)
- Makes code more explicit and easier to understand
- Better for large projects with multiple dependencies

---

### 8. ROS2 Node Not Starting (Linux)

**Symptoms:**
- `Package 'omnihand_node' not found`
- Node fails to communicate

**Solutions:**

```bash
# Load the matching ROS2 environment and OmniHand package automatically
source ros2/setup.bash

# Verify package
ros2 pkg list | grep omnihand

# Check node
ros2 run omnihand_node omnihand_2025_node
```

---

## Diagnostic Commands

### Linux

```bash
# Check USB devices
lsusb -v | grep -A 20 "ZLG\|3068"

# Check serial ports
dmesg | tail -20

# Check udev rules
cat /etc/udev/rules.d/99-omnihand-usb.rules

# Check user groups
groups $USER

# Check SDK import (does not connect to a device)
python3 -c "from omnihand import OmniHand2025; print('OK')"
```

### Windows

```cmd
# Check USB in Device Manager
devmgmt.msc

# Check Python
python --version
pip show omnihand
```

---

## Getting Help

If the issue persists:

1. **Collect information:**
   - OS version
   - SDK version (check `VERSION` file)
   - Error messages (full output)
   - Hardware model (O10/O12, adapter type)

2. **Check logs:**
   Enable C++ SDK TX/RX details on the initialized `hand` instance:

   ```python
   hand.show_data_details(True)
   ```

   Logs are printed to standard output by default. Keep the full TX/RX and error output.

3. **Contact support:**
   - Email: xuqigui@agibot.com
   - Include the collected information
