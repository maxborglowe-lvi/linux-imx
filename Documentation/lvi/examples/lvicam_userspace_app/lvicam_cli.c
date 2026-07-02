#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#include "lvicam.h"

static int data_write(int fd, uint8_t subreg, uint16_t data, size_t size)
{
	struct lvicam_i2c_cmd cmd = {
		.subreg = subreg,
		.data = data,
		.size = size,
	};
	return ioctl(fd, LVICAM_CTRL_IOCTL_WRITE_DATA, &cmd);
}

static void print_help(void)
{
	printf("Usage: lvicam <command>\n\n");
	printf("Commands:\n");
	printf("  zoom_cw         Zoom in (FPGA register)\n");
	printf("  zoom_ccw        Zoom out (FPGA register)\n");
	printf("  zoom_visca_cw   Zoom in (VISCA path)\n");
	printf("  zoom_visca_ccw  Zoom out (VISCA path)\n");
	printf("  zoom_reset      Reset zoom to default position\n");
	printf("  zoom_status     Read current zoom position\n");
	printf("  seesaw          Toggle camera mode (simulate seesaw switch)\n");
	printf("  seesaw_status   Read current seesaw GPIO value\n");
	printf("  natcol          Cycle natural color palette\n");
	printf("  artcol          Cycle artificial color palette\n");
	printf("  func_cw         Function scroll up\n");
	printf("  func_ccw        Function scroll down\n");
	printf("  func_press      Function button press\n");
	printf("  help            Show this help\n");
}

int main(int argc, char *argv[])
{
	int fd;
	int ret = 0;

	if (argc < 2) {
		print_help();
		return 1;
	}

	fd = open("/dev/lvicam", O_RDWR);
	if (fd < 0) {
		perror("Failed to open /dev/lvicam");
		return 1;
	}

	if (strcmp(argv[1], "zoom_cw") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CW, 1);
	} else if (strcmp(argv[1], "zoom_ccw") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CCW, 1);
	} else if (strcmp(argv[1], "zoom_visca_cw") == 0) {
		struct lvicam_i2c_cmd cmd = {
			.subreg = VISCA_CMD_ZOOM_STEP,
			.data = VISCA_CMD_ZOOM_STEP_INC,
			.regtype = REG_TYPE_VISCA,
			.size = 1,
		};
		ret = ioctl(fd, LVICAM_CTRL_IOCTL_WRITE_DATA, &cmd);
	} else if (strcmp(argv[1], "zoom_visca_ccw") == 0) {
		struct lvicam_i2c_cmd cmd = {
			.subreg = VISCA_CMD_ZOOM_STEP,
			.data = VISCA_CMD_ZOOM_STEP_DEC,
			.regtype = REG_TYPE_VISCA,
			.size = 1,
		};
		ret = ioctl(fd, LVICAM_CTRL_IOCTL_WRITE_DATA, &cmd);
	} else if (strcmp(argv[1], "zoom_status") == 0) {
		uint16_t zoom_pos;
		struct lvicam_i2c_cmd cmd = {
			.subreg = CAM1_ZOOM_POS_STATUS_REG,
			.size = 2,
		};
		ret = ioctl(fd, LVICAM_CTRL_IOCTL_READ_DATA, &cmd);
		if (ret == 0) {
			zoom_pos = (uint16_t)cmd.data;
			printf("Zoom position: %u (0x%04X)\n", zoom_pos, zoom_pos);
		}
	} else if (strcmp(argv[1], "zoom_reset") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_DN, 1);
		if (ret == 0)
			ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_UP, 1);
	} else if (strcmp(argv[1], "seesaw") == 0) {
		ret = ioctl(fd, LVICAM_CTRL_IOCTL_TRIGGER_CAMERA_MODE);
	} else if (strcmp(argv[1], "seesaw_status") == 0) {
		struct lvicam_seesaw_status status;
		ret = ioctl(fd, LVICAM_CTRL_IOCTL_READ_SEESAW, &status);
		if (ret == 0)
			printf("Seesaw: %d\n", status.value);
	} else if (strcmp(argv[1], "natcol") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_DN, 1);
		if (ret == 0)
			ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_UP, 1);
	} else if (strcmp(argv[1], "artcol") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_DN, 1);
		if (ret == 0)
			ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_UP, 1);
	} else if (strcmp(argv[1], "func_cw") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CW, 1);
	} else if (strcmp(argv[1], "func_ccw") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CCW, 1);
	} else if (strcmp(argv[1], "func_press") == 0) {
		ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_DN, 1);
		if (ret == 0)
			ret = data_write(fd, USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_UP, 1);
	} else {
		print_help();
		ret = -1;
	}

	if (ret < 0)
		perror("Command failed");

	close(fd);
	return ret == 0 ? 0 : 1;
}
