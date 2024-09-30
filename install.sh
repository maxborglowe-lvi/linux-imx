
#Install the kernel image and modules:
MY_ROOTFS=/home/maxborglowe/var-fsl-yocto/rootfs
kver=$(strings arch/arm64/boot/Image | grep -i "Linux version" | awk 'NR==1{print $3}')
sudo cp arch/arm64/boot/Image.gz /tftpboot/Image.gz-${kver}
sudo ln -fs /tftpboot/Image.gz-${kver} /tftpboot/Image.gz

sudo cp arch/arm64/boot/Image.gz $MY_ROOTFS/boot/Image.gz-${kver}
sudo ln -fs Image.gz-${kver} $MY_ROOTFS/boot/Image.gz


sudo make ARCH=arm64 modules_install INSTALL_MOD_PATH=$MY_ROOTFS

#Install the device trees:
sudo cp arch/arm64/boot/dts/freescale/*imx*var*.dtb /tftpboot/
sudo cp arch/arm64/boot/dts/freescale/*imx*var*.dtb /home/maxborglowe/var-fsl-yocto/rootfs/boot/

