# Instructions

## Wiki
> https://variwiki.com/index.php?title=DART-MX8M-PLUS_Yocto&release=mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1

## Initial setup

### 1. Setup Yocto Project
    * https://variwiki.com/index.php?title=Yocto_Build_Release&release=mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1

          $ cd ~/var-fsl-yocto
          $ MACHINE=imx8mp-var-dart DISTRO=fslc-xwayland . var-setup-release.sh build_xwayland

          # The above command is only mandatory for the very first build setup. 
          # Whenever restarting a newer build session (from a different terminal or in a different time), you can skip the full setup.

          $ source setup-environment build_xwayland
          $ bitbake fsl-image-gui

    * Errors when building?

        * problem with firmware-sof-imx? Add the following:

	          FETCHCMD_wget = "/usr/bin/env wget -t 2 -T 30 --passive-ftp --no-check-certificate --post-data=0"

              # to

	          ~/var-fsl-yocto/sources/meta-freescale/recipes-bsp/firmware-imx/firmware-sof-imx_1.9.0-1.bb

### 2. Install Yocto Toolchain
    * https://variwiki.com/index.php?title=Yocto_Toolchain_installation&release=mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1

          $ cd ~/var-fsl-yocto
          $ source setup-environment build_xwayland
          $ bitbake -c populate_sdk fsl-image-gui

          # Install SDK below and choose default options
          # script name may differ.

          $ ~/var-fsl-yocto/build_xwayland/tmp/deploy/sdk/fslc-xwayland-glibc-x86_64-fsl-image-gui-cortexa53-crypto-imx8mp-var-dart-toolchain-3.3.sh

### 3. Setup TFTP/NFS and rootfs
    * https://variwiki.com/index.php?title=Yocto_Setup_TFTP/NFS&release=mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1

### 4. Prepare the Linux kernel

       $ cd ~/var-fsl-yocto
       $ source setup-environment build_xwayland

       $ bitbake -c patch virtual/kernel
       $ mkdir -p ../local_repos/linux-imx
       $ cp -a tmp/work-shared/imx8mp-var-dart/kernel-source/. ../local_repos/linux-imx
       $ cd ../local_repos/linux-imx
       $ git reset --hard
       $ git clean -fdx

### 5. Apply Daniel Mobergs patches

        $ cd ~/var-fsl-yocto/local_repos/linux-imx 

        $ git am ../../../tmp/patches/0001-Add-tc358746-driver-from-patch.patch
        $ git am ../../../tmp/patches/0002-Update-due-to-kernel-changes-state-instead-of-pad-co.patch
        $ git am ../../../tmp/patches/0003-WIP-Fix-some-compile-errors.patch
        $ git am ../../../tmp/patches/0004-Made-LVI-clone-of-imx8mp-var-dart-dt8mcustomboard.dt.patch
        $ git am ../../../tmp/patches/0005-Add-tc358746-to-i2c-csi0-in-device-tree.patch
        $ git am ../../../tmp/patches/0006-Define-static-reference-clock-used-by-TC358746.patch
        $ git am ../../../tmp/patches/0007-Add-basic-lvicam.patch
        $ git am ../../../tmp/patches/0008-Add-lvicam-to-dts.patch
        $ git am ../../../tmp/patches/0009-Added-missing-callbacks-to-lvicam.patch
        $ git am ../../../tmp/patches/0010-Add-debug-and-1920-conf.patch
        $ git am ../../../tmp/patches/0011-Add-missing-register.patch
        $ git am ../../../tmp/patches/0012-Add-reset.patch
        $ git am ../../../tmp/patches/0013-Use-deafult-format-for-lvicam.patch
        $ git am ../../../tmp/patches/0014-Enable-imx8-debug-hardcode.patch
        $ git am ../../../tmp/patches/0015-Add-support-for-HD.patch
        $ git am ../../../tmp/patches/0016-Implement-power-and-enable-stream-ops.patch
        $ git am ../../../tmp/patches/0017-Use-correct-number-of-lanes-for-hd.patch

**Important: Workng commit for FULL HD is "Enable imx8 debug, hardcode"**

    $ bitbake -c menuconfig virtual/kernel
    $ bitbake -c savedefconfig virtual/kernel

use the generated defconfig and .config below :)

my_defconfig.config should contain CONFIG_VIDEO_LVICAM=y only.

### 6. Build kernel with patches and LVICAM (LVICAM enabled in defconfig)

       source /opt/fslc-xwayland/3.3/environment-setup-cortexa53-crypto-fslc-linux
       export LDFLAGS=

       # Place my_defconfig.config inside ~/var-fsl-yocto/local_repos/linux-imx/arch/arch64/configs

       $ cd ~/var-fsl-yocto/local_repos/linux-imx

       $ make ARCH=arm64 menuconfig
       $ scripts/config --disable SYSTEM_TRUSTED_KEYS
       $ scripts/config --disable SYSTEM_REVOCATION_KEYS
       # save default
       $ make ARCH=arm64 -j8 defconfig imx8_var_defconfig my_defconfig.config
       $ make ARCH=arm64 -j8
       # build device-tree
       make ARCH=arm64 freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dtb 

       # Place install.sh script inside ~/var-fsl-yocto/local_repos/linux-imx

       $ ./install.sh

### 7. Use Picocom

       sudo apt-get install picocom
       sudo picocom -b 115200 /dev/ttyUSB0


### 8. U-boot

Press any key when booting the dev kit to enter u-boot.

https://variwiki.com/index.php?title=U-Boot_4.1.15_features

       => setenv ipaddr 10.42.0.2    // device ip
       => setenv serverip 10.42.0.1  // host ip
       => setenv nfsroot /home/maxborglowe/var-fsl-yocto/rootfs
       => setenv bootcmd 'run netboot'
       # Full HD (working)EXT REF_CLK is used!.
       => setenv fdt_file imx8mp-var-dart-dt8mcustomboard-lvi.dtb
       => saveenv
       => boot

        setenv bootargs console=${console} root=/home/maxborglowe/var-fsl-yocto/rootfs rw nfsroot=${serverip}:${nfsroot},nfsvers=4,tcp ip=${ipaddr}:${serverip}:<gateway_addr>:<netmask>::eth0(or your device):off

       ..

       # HD
       => setenv fdt_file imx8mp-var-dart-dt8mcustomboard-lvi-hd.dtbum

       reset?

       => env default -a 
       => saveenv 
       # restart the device after the saveenv

9. Verify LVICAM

       $ dmesg | grep lvicam

10. Run GStreamer

        $ gst-launch-1.0 v4l2src device=/dev/video1 ! video/x-raw,width=1920,height=1080,format=NV12 ! waylandsink

        # run in background without printouts:
        gst-launch-1.0 v4l2src device=/dev/video1 ! video/x-raw,width=1920,height=1080,format=NV12 ! waylandsink >/dev/null 2>&1 &


## External files

### Install.sh script

    G:\Utveckling (HB)\Johannes\ZIP NXTG\install.sh

Place install.sh script inside ~/var-fsl-yocto/local_repos/linux-imx

### Patches

    G:\Utveckling (HB)\Johannes\ZIP NXTG\patches\

### Defconfig

    G:\Utveckling (HB)\Johannes\ZIP NXTG\my_defconfig.config

Place my_defconfig.config inside ~/var-fsl-yocto/local_repos/linux-imx/arch/arch64/configs

#### display on multiple monitors

    # (to get connector-id)
    $ modetest 

    $ gst-launch-1.0 videotestsrc ! kmssink can-scale=false connector-id=40



https://community.nxp.com/t5/i-MX-Processors/Change-resolution-at-runtime-on-iMX8/m-p/1078747
1.  To change  output mode to your resolution at "/etc/xdg/weston/weston.ini"

[output]
name=HDMI-A-1                       
mode=1920x1080@60

2. restart weston 

  systemctl restart weston



https://community.nxp.com/t5/i-MX-Processors/Using-HDMI-as-mirror-of-primary-lvds-display/m-p/1397527


Blacklisted plugin?

    $ rm ~/.cache/gstreamer-1.0/registry* 



gst-launch-1.0 v4l2src device=/dev/video1 ! video/x-raw, format=YUY2, width=1920, height=1080, framerate=60/1 ! multifilesink location=frame_%05d.yuy2 -e
scp root@10.42.0.232:/home/root/test/frame_00037.yuy2 ~/Linux-Kenneth/tmp/

gst-launch-1.0 v4l2src device=/dev/video1 ! video/x-raw, format=RGB16, width=1920, height=1080, framerate=60/1 ! multifilesink location=frame_%05d.rgb565 -e
scp root@10.42.0.232:/home/root/test/frame_00037.rgb565 ~/Linux-Kenneth/tmp/



https://rawpixels.net/

#### USB Camera

1. USB Mouse (cursor) must be present at display.

    gst-launch-1.0 v4l2src device=/dev/video1 ! videoconvert ! videorate ! video/x-raw,framerate=30/1 ! autovideosink

#### Mapping touch screen to display

https://community.toradex.com/t/weston-wayland-usb-input-mapping/21817/4

cd /etc/udev/rules.d
sudo nano touchscreen.rules 

VID: 0x2575
PID: 0xc300

Bus 001 Device 006

ENV{ID_VENDOR_ID}=="<2575>",ENV{ID_MODEL_ID}=="<C300>",DEVPATH=="/devices/platform/soc@0/32f10100.usb/38100000.dwc3/xhci-hcd.0.auto/usb1/1-1/1-1.1/1-1.1.3/*",ENV{WL_OUTPUT}="<HDMI-A-1>"


/devices/platform/soc@0/32f10100.usb/38100000.dwc3/xhci-hcd.0.auto/usb1/1-1/1-1.1/1-1.1.1/

## Create Bootable Recovery SD Card (ALL YOU NEED TO BEGIN YOUR JOURNEY)

### Yocto Revovery SD Card

https://dev.variscite.com/dart-mx8m-plus/mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1/yocto-recovery-sd-card/

Download image from Variscite: https://variscite-public.nyc3.cdn.digitaloceanspaces.com/DART-MX8M-PLUS/Software/mx8mp__yocto-hardknott-5.10.72_2.2.1-v1.1__android-11.0.0_2.6.0-v1.2.img.gz

Connect SD card and mount on Linux host PC, then copy the image:
    $ sudo umount /dev/sdX?*
    $ zcat <image name>.img.gz | sudo dd of=/dev/sdX bs=1M && sync

--->$ zcat mx8mp__yocto-hardknott-5.10.72_2.2.1-v1.1__android-11.0.0_2.6.0-v1.2.img.gz | sudo dd of=/dev/sdX bs=1M && sync    

Next, plug in the SD card on the ZIP-NXTG carrierboard and put the BOOT MODE switch in "SD".
Boot the device, and login. Run "install_yocto.sh" from /usr/bin

    $ ./usr/bin/install_yocto.sh

Now set the BOOT MODE switch in "MMC" mode, and press the "RST SOM" button. The system will now boot into the installed Yocto system.
However, to apply your kernel files and DTS, you must follow the [U-Boot section](#8-u-boot).

### Android Recovery SD Card




# NOT SURE WHAT TO USE THIS FOR
----------------------------------------------

### Copy Image to bootable SD card 

https://variwiki.com/index.php?title=Yocto_Build_Linux&release=mx8mp-yocto-hardknott-5.10.72_2.2.1-v1.1#Install_the_built_kernel_images,_modules,_and_device_trees_on_an_SD_card

~/var-fsl-yocto/rootfs

Copy the Image.gz and device trees to the SD card boot partition, and install the modules in the SD card rootfs partition.
Assuming the rootfs partition is mounted on /media/root:

Install the kernel image and modules:
    WORKDIR=~/var-fsl-yocto/local_repos
    cd ${WORKDIR}/linux-imx
    kver=$(strings arch/arm64/boot/Image | grep -i "Linux version" | awk 'NR==1 {print $3}')
    sudo cp arch/arm64/boot/Image.gz /media/root/boot/Image.gz-${kver}
    sudo ln -fs /boot/Image.gz-${kver} /media/root/boot/Image.gz
    sudo cp -r ~/var-fsl-yocto/rootfs/* /media/root
    sudo rsync -Kra $WORKDIR/../rootfs/* /media/root

    sudo cp ~/var-fsl-yocto/local_repos/linux-imx/arch/arm64/boot/dts/freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dtb /media/root/boot/

Install the device trees:
    sudo cp ~/var-fsl-yocto/local_repos/linux-imx/arch/arm64/boot/dts/freescale/*imx*var*.dtb /media/root/boot/




$ cd /home/maxborglowe/var-fsl-yocto/local_repos/linux-imx
$ kver=$(strings arch/arm64/boot/Image | grep -i "Linux version" | awk 'NR==1 {print $3}')
$ sudo cp arch/arm64/boot/Image.gz /media/maxborglowe/rootfs/boot/Image.gz-${kver}
############### $ sudo ln -fs /boot/Image.gz-${kver} /media/maxborglowe/rootfs/boot/Image.gz
$ sudo cp /home/maxborglowe/var-fsl-yocto/rootfs/* /media/maxborglowe/rootfs

Install the device trees:
$ sudo cp arch/arm64/boot/dts/freescale/*imx*var*.dtb /media/maxborglowe/rootfs/boot/