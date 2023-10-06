
## SETUP Touch-screen display ##

source: https://wiki.tizen.org/IVI/Mapping_multiple_touchscreen_Wayland

1.  Connect the display, and leave the USB touch-screen unconnected.

2.  Open up a terminal window and log in to the Yocto device.
    See picocom instructions in /home/maxborglowe/var-fsl-yocto/local_repos/linux-imx/instructions.md

    Take note of the following parameters, which will later be used to map the touch-screen to its corresponding display:
    - <display_name>
    - <vendor_id>
    - <product_id>
    - <devpath>

2.  Find the <display_name> of the display you want to be affected by the touch-screen by inputting the following commands in terminal:
    $ cd /etc/xdg/weston
    $ sudo nano weston.ini

    The file will contain elements such as this:
    
    [output]
    name=HDMI-A-1 <----- the <display_name> is HDMI-A-1
    mode=1920x1080@60
    scale=1
    transform=normal

3.  Find the <devpath>:
    $ udevadm monitor

    Now, plug in the USB touch-screen and something like the following should be printed:

    ...
    KERNEL[893.529419] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1 (usb)
    KERNEL[893.587207] change   /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1 (usb)
    KERNEL[893.587591] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0 (usb)
    KERNEL[893.590150] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002 (hid)
    KERNEL[893.592315] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002/input/input5 (input)
    KERNEL[893.592449] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002/input/input5/event2 (input)
    KERNEL[893.592544] add      /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002/hidraw/hidraw0 (hidraw)
    KERNEL[893.592623] bind     /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002 (hid)
    KERNEL[893.592706] bind     /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0 (usb)
    KERNEL[893.592794] bind     /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1 (usb)
    ...

                                /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/* <----- This is the <devpath>.
                                Note: the * limits how precisely you want to define where the touch-screen is physically connected.
                                Setting ../usb1/1-1/* lets the touch-screen be detected on any of the USB-slots in USB port 1.
                                Setting ../usb1/1-1/1-1.1/* only slot 1 in USB port 1 allowed.
                                Setting ../usb1/1-1/1-1.2/* only slot 2 in USB port 1 allowed.

4.  With the USB touch-screen connected, find the <vendor_id> and <product_id> of the touch display:

    $ cat /proc/bus/input/devices

    This prints all connected input devices. Find the device that matches your touch-screen:

    ...
    I: Bus=0003 Vendor=2575 Product=c301 Version=0110 <----- Here is the <vendor_id> and <product_id>
    N: Name="Weida Hi-Tech CoolTouch® System"
    P: Phys=usb-xhci-hcd.1.auto-1.1/input0
    S: Sysfs=/devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/1-1.1/1-1.1:1.0/0003:2575:C301.0002/input/input5
    U: Uniq=
    H: Handlers=event2 
    B: PROP=2
    B: EV=1b
    B: KEY=400 0 0 0 0 0
    B: ABS=260800000000003
    B: MSC=20
    ...

5. Now that we have gathered all the information, we now need to create a .rules file to map the touch-screen to our display.

    - <display_name>    HDMI-A-1
    - <vendor_id>       2575
    - <product_id>      c301
    - <devpath>         /devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/*

    $ cd /etc/udev/rules.d
    $ touch multi-touch.rules
    $ sudo nano multi-touch.rules

    This creates and opens a file called multi-touch.rules.
    Add the following line, and replace the parameters enclosed in "" with the obtained values:

    ENV{ID_VENDOR_ID}=="<vendor_id>",ENV{ID_MODEL_ID}=="<product_id>",ENV{WL_OUTPUT}="<display_name>",DEVPATH=="<devpath>"

    Result:

    ENV{ID_VENDOR_ID}=="2575",ENV{ID_MODEL_ID}=="c301",ENV{WL_OUTPUT}="HDMI-A-1",DEVPATH=="/devices/platform/soc@0/32f10108.usb/38200000.dwc3/xhci-hcd.1.auto/usb1/1-1/*"

    Save and exit GNU nano.

6. Reboot Yocto to execute changes.

    $ reboot