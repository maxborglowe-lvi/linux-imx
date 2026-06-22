# 4K HDMI Video Corruption Diagnostic

## Symptom
- LT6911 detects: `UYVY8_2X8/3840x2160@1/27` (extremely low framerate)
- Gstreamer shows 26-27 fps (frame drops and potential corruption)
- Video output looks corrupted

## Root Cause Analysis Needed

### 1. Query Actual DV Timings from LT6911
```bash
# First, find the correct LT6911 subdev number
v4l2-ctl -d /dev/v4l-subdev3 --query-dv-timings

# Check if timing detection succeeded and what it detected
v4l2-ctl -d /dev/v4l-subdev3 --get-dv-timings

# If manual setting needed:
v4l2-ctl -d /dev/v4l-subdev3 --set-dv-bt-timings query
```

**Expected output should show:** 3840x2160@30 (not @1/27)

### 2. Check Kernel Logs for LT6911 and MIPI Errors
```bash
# Check for hotplug detection and timing lock issues
dmesg | tail -100 | grep -i "lt6911\|lt6911uxc\|mipi\|csi"

# Look for:
# - HDMI hotplug detection
# - Timing lock success/failure
# - MIPI clock/lane errors
# - ISI buffer overflow warnings
```

### 3. Verify ISI Pipeline Configuration
```bash
# Check if ISI is properly configured for 4K
v4l2-ctl -d /dev/video2 --get-fmt-video

# Expected: width=3840, height=2160, pixelformat=NV12 (or YUY2)
# If showing 1920x1080 or lower, ISI scaler is interfering

# Check ISI capabilities
v4l2-ctl -d /dev/video2 --list-framesizes=NV12

# Try explicit format/framerate setting BEFORE gstreamer:
v4l2-ctl -d /dev/video2 --set-fmt-video=width=3840,height=2160,pixelformat=NV12
v4l2-ctl -d /dev/video2 --set-parm=type=1,numerator=1,denominator=30
```

### 4. Capture Raw Frames for Analysis
```bash
# Capture to file WITHOUT display pipeline to isolate source vs. sink
gst-launch-1.0 -e v4l2src device=/dev/video2 io-mode=4 num-buffers=30 \
  ! video/x-raw,width=3840,height=2160,format=NV12,framerate=30/1 \
  ! multifilesink location=/tmp/frame_%05d.nv12

# Check captured frame size and count
ls -lh /tmp/frame_*.nv12 | wc -l

# Expected: ~30 frames
# If much fewer, ISI is dropping/buffering incorrectly
```

### 5. Check ISI Memory Bandwidth and Buffer Configuration
```bash
# Monitor system load during 4K capture
while true; do cat /proc/stat | grep cpu; sleep 1; done &
BGPID=$!

gst-launch-1.0 v4l2src device=/dev/video2 io-mode=4 num-buffers=300 \
  ! video/x-raw,width=3840,height=2160,format=NV12,framerate=30/1 \
  ! fpsdisplaysink video-sink=fakesink sync=false text-overlay=false

kill $BGPID

# Check for dmesg errors during/after test
dmesg | tail -50
```

### 6. Test with Explicit MIPI CSI Tuning
If the above shows timing issues, the ISI settle times may need adjustment.

**Current DTS settings (in arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dts):**
```
&mipi_csi_1 {
    port@1 {
        csis-hs-settle = <19>;
        csis-clk-settle = <2>;
    };
};
```

**If frames are corrupted at start/end**, try increasing settle times:
```
csis-hs-settle = <25>;
csis-clk-settle = <4>;
```

Then rebuild device tree:
```bash
make ARCH=arm64 freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dtb
./install.sh
reboot
```

### 7. Verify HDMI Cable and Source Stability
```bash
# Check if HDMI input is stable (no repeated hotplug)
dmesg | grep -i "hotplug\|disconnect\|connect" | tail -20

# If seeing repeated disconnect/reconnect, issue is with HDMI handshake
```

## Investigation Priority
1. **Run step 1** → Confirm actual DV timing detected
2. **Run step 2** → Look for kernel errors during detection
3. **Run step 3** → Verify ISI frame format (must be 3840x2160, not downscaled)
4. **Run step 4** → Isolate capture quality from display (fakesink eliminates Wayland bottleneck)
5. **Run step 5** → Check for buffer/bandwidth issues
6. **Provide full dmesg output** and the outputs of steps 1-5

## Questions for Next Steps
- What HDMI camera model is sending the 4K30 stream?
- Have you tested the same camera with standard 1920x1080 HDMI input successfully?
- Is the ISI downscaling to 1920x1080 before display, or should it pass 3840x2160 native?
