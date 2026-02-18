#!/bin/bash
source /opt/fslc-xwayland/3.3/environment-setup-cortexa53-crypto-fslc-linux
export LDFLAGS=

# Load the default config first, then apply your custom config
make ARCH=arm64 defconfig imx8_var_defconfig my_defconfig.config
make ARCH=arm64

# Build the device tree for LVI carrierboard
make ARCH=arm64 freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dtb

# Build the device tree for Variscite carrierboard
# make ARCH=arm64 freescale/imx8mp-var-dart-dt8mcustomboard-variscite.dtb


./install.sh
