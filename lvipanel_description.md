### INTRODUCTION

The LVI panel is a hardware interface that lets a user control functions of an LVI system using a set of knobs and buttons.
The panel communicates via I2C with the host system.

Currently, the software and communication is set up accordingly:

- lvipanel.c: A kernel module that handles I2C communication with the LVI panel. Current location: /var-fsl-yocto/local_repos/linux-imx/drivers/input/misc/lvipanel.c
- lvipanel_commands.h: Pre-defined I2C commands for use with the LVI panel. Located in the same dir as kernel module.
- lviapp.c: A user space application that reads/writes pre-defined commands (see lvipanel_commands.h) to the kernel module (lvipanel.c).
- lvi-startup.service: Service that executes the application (lviapp.c) upon startup. Current location (on iMX8): /etc/systemd/system/lvi-startup.service

## STARTUP AND SHUTDOWN/REBOOT
# STARTUP
The kernel module is built-in. Upon startup, the LVI panel is probed for a response.


# SHUTDOWN/REBOOT
 Upon shutdown or reboot, a script called "99-lvi-shutdown.sh" located in /lib/systemd/system-shutdown is called.
 This script removes the lvipanel module, causing the module to execute removal commands, i.e. flashing lights, turning lights off, etc.