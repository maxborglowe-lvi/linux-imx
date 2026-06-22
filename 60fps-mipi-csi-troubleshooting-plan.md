# 60fps MIPI-CSI Video Troubleshooting Plan

## Problem Statement
Running GStreamer on `/dev/video2` (LVICAM MIPI-CSI source, 1080p) achieves only ~45fps with occasional video glitches (<1ms hangs). Target: Achieve stable 60fps with zero glitches.

### Current Command
```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
! fpsdisplaysink video-sink=waylandsink sync=false text-overlay=true
```

---

## Phase 1: Root Cause Isolation (Kernel vs Userspace)

### 1.1 Verify Driver Capabilities

**Check LVICAM driver configuration** in `drivers/media/i2c/lvicam.c`:
- Confirm hardcoded 1920x1080 UYVY @ 60fps
- Verify no frame-drop logic or clock throttling in the driver

**Verify Device Tree bindings** in `arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dts`:
- Check link-frequencies (should allow 60fps at 1920x1080 UYVY)
- Verify ISI (Image Sensor Interface) capture device is enabled
- Check MIPI-CSI lane count (4-lane link needed for 1080p60)

**Test raw driver throughput** (kernel-level):
```bash
v4l2-ctl --device=/dev/video2 --list-formats-ext
media-ctl -d /dev/media0 --print-topology
```

### 1.2 Isolate Capture Layer (No Conversion/Display)

Run a series of progressive tests to identify the bottleneck:

#### Test 1a: Fakesink (no conversion)
```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 num-buffers=600 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! fpsdisplaysink video-sink=fakesink sync=false text-overlay=false
```

**Expected Result**: If driver is correct, should see stable 60fps here
- **If <60fps**: Kernel driver issue → Proceed to Phase 2
- **If ≥60fps**: Proceed to Test 1b

#### Test 1b: NV12 format (if available)
```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 num-buffers=600 \
  ! video/x-raw,format=NV12,width=1920,height=1080,framerate=60/1 \
  ! fpsdisplaysink video-sink=fakesink sync=false text-overlay=false
```

**Actions**:
- If NV12 available: Compare fps with YUY2
- If unavailable: Note in findings

---

## Phase 2: Identify Bottleneck (If Kernel Issue Suspected)

### 2.1 Check Power/Clock Constraints

**Verify VCAM power supply voltage**:
- Check if digipot (MCP40D18T at I2C 0x3e) is configured correctly
- Confirm VDISP rail is stable at full voltage during capture
- Reference: `drivers/iio/potentiometer/mcp40d18.c`

**Monitor CPU/thermal state during capture**:
```bash
cat /sys/class/thermal/thermal_zone0/temp
watch -n 0.1 'cat /proc/cpuinfo | grep MHz'
```

**Actions**:
- Check for frequency scaling/throttling
- If thermal throttling detected, increase thermal limits or improve cooling

### 2.2 Inspect ISI/CSI Driver Buffers

**Check kernel logs for buffer drops**:
```bash
dmesg | tail -100 | grep -i "mxc_isi\|mipi\|drop\|error"
```

**Check sysfs for rate-limiting or error counters**:
```bash
ls -la /sys/devices/platform/*/
```

### 2.3 Test at Lower Framerate

```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=30/1 \
  ! fpsdisplaysink video-sink=fakesink sync=false text-overlay=false
```

**Actions**:
- If 30fps is stable with no drops → Driver can sustain at least 30fps
- Incrementally test: 45fps → 48fps → 50fps → 55fps to find the edge
- Document max sustainable framerate

---

## Phase 3: Fix Userspace Pipeline (If Kernel ≥60fps)

### 3.1 Replace Wayland with KMS Sink

```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! kmssink
```

**Rationale**: Wayland composition can drop frames; KMS is lower-level and more deterministic.

### 3.2 Use Hardware Converter (imxvideoconvert_g2d)

```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! imxvideoconvert_g2d \
  ! video/x-raw,format=RGB16,width=1920,height=1080 \
  ! fpsdisplaysink video-sink=waylandsink sync=false text-overlay=true
```

**Rationale**: Hardware converter offloads YUY2→RGB conversion from CPU.

### 3.3 Optimize Buffer Pool

```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 num-buffers=8 \
  ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=0 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! fpsdisplaysink video-sink=waylandsink sync=false text-overlay=true
```

**Rationale**: Adjust queue parameters to prevent congestion and buffer starvation.

---

## Phase 4: Diagnose & Fix Video Hangs (<1ms)

### 4.1 Capture GStreamer Debug Info

```bash
GST_DEBUG=3 gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! fpsdisplaysink video-sink=waylandsink sync=false text-overlay=true 2>&1 | tee gst_debug.log
```

**Search log for**: `dropped`, `overrun`, `underrun`, `negotiation`, `state-change`

### 4.2 Check for Interrupt/IRQ Issues

```bash
watch -n 1 'cat /proc/interrupts | grep -i "csi\|isi\|dma"'
```

**Actions**:
- Look for spikes in interrupt counts that correlate with hangs
- If spikes detected, check IRQ affinity and handler timing

### 4.3 Monitor DMA Memory Pressure

```bash
cat /proc/meminfo | grep -i "dirty\|writeback"
watch -n 0.1 'free -h'
```

**Rationale**: Hangs <1ms often indicate memory stalls or DMA buffer exhaustion.

---

## Phase 5: Driver-Level Fixes (If Needed)

### 5.1 Check LVICAM Driver Clock Configuration

**In `drivers/media/i2c/lvicam.c`**:
- Verify MIPI clock is not being gated mid-stream
- Check for CONFIG_PM or runtime PM that might pause capture
- Inspect enable/disable clock sequences

### 5.2 Review ISI Buffer Management

**In `drivers/media/platform/imx8-isi/imx8-isi-cap.c`**:
- Buffer allocation strategy (contiguous vs. scattered DMA)
- Any frame-skip or drop-on-overflow logic
- IRQ handler timing and latency

**Key areas to audit**:
- `imx8_isi_capture_hw_init()`
- `imx8_isi_cap_frame_write()`
- DMA descriptor chain setup

### 5.3 Test Device Tree Clock Assignments

**In `arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dts`**:
- Verify no conflicting clock sources feeding ISI
- Confirm parent clock frequency supports exact 60fps denominator
- Check link-frequencies array for correct MIPI clock setting

---

## Phase 6: Validation & Performance Tuning

### 6.1 Extended Stress Test

```bash
timeout 120 gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 num-buffers=7200 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! fpsdisplaysink video-sink=<final-sink> sync=false text-overlay=true \
  2>&1 | grep -E "fps|drop"
```

**Test Duration**: 120 seconds (7200 frames @ 60fps)

**Success Criteria**:
- Stable 60fps throughout
- <3 frame drops total
- No hang events

### 6.2 Optional: Add Frame Timestamp Logging

```bash
gst-launch-1.0 -v v4l2src device=/dev/video2 io-mode=4 \
  ! video/x-raw,format=YUY2,width=1920,height=1080,framerate=60/1 \
  ! tee name=t \
  t. ! queue ! fpsdisplaysink video-sink=waylandsink sync=false \
  t. ! queue ! filesink location=/tmp/capture.raw
```

**Analysis**:
- Examine `/tmp/capture.raw` frame timing for jitter
- Calculate frame interval statistics (should be ≈16.67ms ± <1ms)

---

## Success Criteria Checklist

| Phase | Test | Expected Result | Failure Action |
|-------|------|-----------------|-----------------|
| 1a | Fakesink YUY2 | 60fps stable, 0 drops | → Phase 2 kernel debug |
| 1b | NV12 format | Match YUY2 or ≥55fps | Document as unavailable |
| 2.1 | Power check | VCAM voltage stable, no throttle | Adjust digipot or thermal config |
| 2.2 | ISI buffers | No kernel drops in dmesg | Debug IRQ handler or DMA config |
| 3.1 | KMS sink | ≥58fps with kmssink | → Try imxvideoconvert_g2d |
| 4.1 | GStreamer logs | <10 drops over 2 min | Correlate with interrupt spikes |
| 6.1 | Stress test | 120s @ 60fps, <3 drops | Root cause fix in Phase 5 |

---

## Key Assumptions

- `/dev/video2` is the correct LVICAM device
- Kernel has `v4l2-ctl`, `media-ctl`, and GStreamer installed
- Device tree has ISI capture enabled
- No hardware defects in MIPI connector or power delivery
- Sufficient memory for buffering at 1920x1080 YUY2 60fps (~540MB/s)

---

## Important References

- **LVICAM Driver**: `drivers/media/i2c/lvicam.c`
- **ISI Capture**: `drivers/media/platform/imx8-isi/imx8-isi-cap.c`
- **Device Tree**: `arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dts`
- **MCP40D18 Digipot**: `drivers/iio/potentiometer/mcp40d18.c`
- **Media Controller**: `drivers/media/mc/media-device.c`

---

## Notes for GPT5.4 Execution

1. **Systematic Execution**: Follow phases sequentially; do not skip diagnostic steps.
2. **Error Handling**: If any test fails, clearly document the failure mode before moving to the next phase.
3. **Code Inspection**: When examining driver code, prioritize areas flagged in this plan.
4. **No Speculative Fixes**: Only modify code if root cause has been isolated in testing.
5. **Verification**: After any fix, re-run the corresponding test from Phase 1 or 6 to confirm resolution.
6. **Documentation**: Keep a log of all test results, kernel messages, and any code changes made.

---

**Generated**: 2026-04-24  
**Target System**: NXP IMX8MP, Yocto/LF-5.10.y kernel  
**Camera**: LVICAM via MIPI-CSI2 (1920x1080 @ 60fps)
