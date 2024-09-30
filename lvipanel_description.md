### INTRODUCTION

The LVI panel is a hardware interface that lets a user control functions of an LVI system using a set of knobs and buttons.
The panel communicates via I2C with the host system.

Currently, the software and communication is set up accordingly:

- lvipanel.c: A kernel module that handles I2C communication with the LVI panel. Current location: /var-fsl-yocto/local_repos/linux-imx/drivers/input/misc/lvipanel.c
- lvipanel_commands.h: Pre-defined I2C commands for use with the LVI panel. Located in the same dir as kernel module.
- lviapp.c: A user space application that reads/writes pre-defined commands (see lvipanel_commands.h) to the kernel module (lvipanel.c).

## STARTUP AND SHUTDOWN/REBOOT
# STARTUP
The kernel module is built-in. Upon startup, the LVI panel is probed via I2C for a response.
If the kernel module receives a response from the panel, the module continues to exchange commands with the panel. As to establish if the system has turned on, off, etc.

If no module is detected, this means that only the LVI Power Button is connected to the system.
(TODO:) In such case, the pins that transfer I2C data to/from the panel should change mode into PWM. This will enable the kernel module to control the lighting scheme of the button.


# SHUTDOWN/REBOOT

When the system has turned on, a user space application called "lviapp" is loaded. The loading is executed by a service file "/etc/systemd/system/lvi-startup.service".
The lviapp awaits signals from the LVI panel/power button.
If the system registers that a shutdown has been initiated by pressing the LVI Power Button, the application will alert this to the LVI Panel.
The LVI Panel consequently returns to its initial state, awaiting the system to turn on again.
