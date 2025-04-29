#!/bin/bash

# Install the kernel image and modules
WORKDIR=~/var-fsl-yocto/local_repos/linux-imx
kver=$(strings ${WORKDIR}/arch/arm64/boot/Image | grep -i "Linux version" | awk 'NR==1 {print $3}')
sudo cp ${WORKDIR}/arch/arm64/boot/Image.gz /media/maxborglowe/rootfs/boot/Image.gz-${kver}
sudo ln -fs ${WORKDIR}/boot/Image.gz-${kver} /media/maxborglowe/rootfs/boot/Image.gz
sudo cp -r ~/var-fsl-yocto/rootfs/* /media/maxborglowe/rootfs

# Install the device trees
sudo cp ${WORKDIR}/arch/arm64/boot/dts/freescale/imx*var*.dtb /media/maxborglowe/rootfs/boot/
