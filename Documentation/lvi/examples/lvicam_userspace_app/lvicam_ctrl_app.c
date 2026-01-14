#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "lvicam_ctrl.h" //must be identical to the kernel header file "lvicam_ctrl.h"

// Include necessary headers for interrupt signal handling
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h> // for waitpid

#include "panel_events.h"

// ===== LVIPANEL DEFINITIONS =====
// If you have a lvipanel.h header, include it instead of these definitions
#define DEVICE_NAME "/dev/lvipanel"
#define IOCTL_READ_DATA _IOR('i', 1, char)
#define IOCTL_WRITE_DATA _IOW('i', 2, char)
#define DATA_AVAILABLE_SIGNAL SIGUSR1

static int fd = -1;
static int panel_fd = -1;

// Status variables
static uint8_t object_show_view = 0;
static uint16_t fpga_status;
static uint16_t camera_id = CAM_ID_WONWOO_MC108_M3;
static uint16_t camera1_zoom_pos;

// Panel control variables
static int keep_running = 1;
static char panel_event = PANEL_EVENT_IDLE;

// GStreamer control
static pid_t gstreamer_pid = -1;

/** @brief write n bytes of data to an i2c subregister on the FPGA
 * @param subreg The subregister to read from
 * @param out_value Pointer to store the read value
 * @param size The size of the data to read (1 or 2 bytes)
 * @return 0 on success, negative value on error
 */
static int data_write(uint8_t subreg, uint16_t data, size_t size)
{
    struct lvicam_i2c_cmd cmd = {
        .subreg = subreg,
        .data = data,
        .size = size,
    };

    int ret = ioctl(fd, LVICAM_CTRL_IOCTL_WRITE_DATA, &cmd);
    if (ret < 0)
        perror("ioctl failed");

    if (size == 1)
        printf("Sent to subreg 0x%02X: data 0x%02X\n", subreg, data);
    else if (size == 2)
        printf("Sent to subreg 0x%02X: data 0x%04X\n", subreg, (uint16_t)data);

    return ret;
}

/** @brief read n bytes of data from an i2c subregister on the FPGA
 * @param subreg The subregister to read from
 * @param out_value Pointer to store the read value
 * @param size The size of the data to read (1 or 2 bytes)
 * @return 0 on success, negative value on error
 */
static int data_read(uint8_t subreg, uint16_t *out_value, uint8_t size)
{
    struct lvicam_i2c_cmd cmd = {
        .subreg = subreg,
        .data = 0,
        .size = size,
    };

    int ret = ioctl(fd, LVICAM_CTRL_IOCTL_READ_DATA, &cmd);
    if (ret < 0)
    {
        perror("ioctl read failed");
        return ret;
    }

    *out_value = cmd.data;
    if (size == 1)
        printf("Read from subreg 0x%02X: data 0x%02X\n", subreg, (uint8_t)cmd.data);
    else
        printf("Read from subreg 0x%02X: data 0x%04X\n", subreg, cmd.data);

    return 0;
}

static int lvicam_read_seesaw(struct lvicam_seesaw_status *seesaw_status)
{

    int ret = ioctl(fd, LVICAM_CTRL_IOCTL_READ_SEESAW, seesaw_status);
    if (ret < 0)
    {
        perror("ioctl LVICAM_CTRL_IOCTL_READ_SEESAW failed");
        return ret;
    }
    printf("Seesaw status: %d\n", seesaw_status->value);
    return 0;
}

/* ########### SETTERS ############### */

/** @brief Increment the zoom position command for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_zoom_in()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CW, 1);
    return ret;
}

/** @brief Decrement the zoom position command for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_zoom_out()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_CCW, 1);
    return ret;
}

/** @brief Set the zoom step amount for the camera, i.e. how much the zoom position changes with each increment
 * @param step The zoom step amount to set (0-255)
 * @return 0 on success, negative value on error
 */
int lvicam_zoom_step_set(uint16_t step)
{
    int ret = data_write(CAM1_ZOOM_POS_INC_STEP_CONTROL_REG, step, 2);
    return ret;
}

/** @brief Reset the zoom position to the default value
 * @return 0 on success, negative value on error
 */
int lvicam_zoom_reset()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_DN, 1);
    ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ZOOM_PUSH_UP, 1);
    return ret;
}

/** @brief Increment the natural color command for the camera (switches between natural and bw color)
 * @return 0 on success, negative value on error
 */
int lvicam_natcol_next()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_DN, 1);
    ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_NATCOL_UP, 1);
    return ret;
}

/** @brief Increment the artificial color command for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_artcol_next()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_DN, 1);
    ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_ARTCOL_UP, 1);
    return ret;
}

/** @brief Scroll up in the list of functions for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_function_scroll_up()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CW, 1);
    return ret;
}

/** @brief Scroll down in the list of functions for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_function_scroll_down()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_CCW, 1);
    return ret;
}

/** @brief Increment the currently used function in the list of functions for the camera
 * @return 0 on success, negative value on error
 */
int lvicam_function_next()
{
    int ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_DN, 1);
    ret = data_write(USER_PANEL1_CMD_CONTROL_REG, CMD_FUNCTION_BTN_UP, 1);
    return ret;
}

int lvicam_set_camera_id()
{
    int ret;
    ret = data_write(CAM_ID_STATUS_REG, camera_id, 2);
    return ret;
}

/* ########### GETTERS ############### */
// getter functions not implemented on FPGA yet.

/** @brief  */
int lvicam_get_fpga_status()
{
    int ret;
    ret = data_read(FPGA_FLAGS_INIT_STATUS_REG, &fpga_status, 1);
    printf("FPGA status: %u\n", fpga_status);

    return ret;
}

int lvicam_get_camera_id()
{
    int ret;
    ret = data_read(CAM_ID_STATUS_REG, &camera_id, 2);
    printf("Camera ID: %u\n", camera_id);

    return ret;
}

int lvicam_get_camera1_zoom_pos_status()
{
    int ret;
    ret = data_read(CAM1_ZOOM_POS_STATUS_REG, &camera1_zoom_pos, 2);
    printf("Camera1 zoom position: %u\n", camera1_zoom_pos);

    return ret;
}

/* ########### GSTREAMER CONTROL ############### */

/** @brief Start GStreamer video pipeline
 */
void gstreamer_start()
{
    if (gstreamer_pid > 0)
    {
        printf("GStreamer already running with PID: %d\n", gstreamer_pid);
        return;
    }

    gstreamer_pid = fork();

    if (gstreamer_pid == 0)
    {
        // Child process - execute GStreamer command
        printf("Starting GStreamer pipeline...\n");
        execlp("gst-launch-1.0", "gst-launch-1.0",
               "v4l2src", "device=/dev/video1", "!",
               "video/x-raw,width=1920,height=1080,format=NV12", "!",
               "waylandsink",
               NULL);

        // If execlp fails
        perror("Failed to execute gst-launch-1.0");
        exit(EXIT_FAILURE);
    }
    else if (gstreamer_pid > 0)
    {
        // Parent process
        printf("GStreamer started with PID: %d\n", gstreamer_pid);
    }
    else
    {
        perror("Failed to fork for GStreamer");
        gstreamer_pid = -1;
    }
}

/** @brief Stop GStreamer video pipeline
 */
void gstreamer_stop()
{
    if (gstreamer_pid <= 0)
    {
        printf("GStreamer is not running\n");
        return;
    }

    printf("Stopping GStreamer (PID: %d)...\n", gstreamer_pid);

    // Send SIGTERM for graceful shutdown
    if (kill(gstreamer_pid, SIGTERM) == 0)
    {
        // Wait for process to terminate
        int status;
        waitpid(gstreamer_pid, &status, 0);
        printf("GStreamer stopped\n");
        gstreamer_pid = -1;
    }
    else
    {
        perror("Failed to stop GStreamer");
    }
}

/** @brief Toggle GStreamer video pipeline on/off
 */
void gstreamer_toggle()
{
    if (gstreamer_pid > 0)
    {
        printf("Toggling GStreamer OFF\n");
        gstreamer_stop();
    }
    else
    {
        printf("Toggling GStreamer ON\n");
        gstreamer_start();
    }
}

/* ########### PANEL EVENT FUNCTIONS ############### */

const char *panel_event_to_string(char event)
{
    switch (event)
    {
    case PANEL_EVENT_IDLE:
        return "PANEL_EVENT_IDLE";
    case PANEL_EVENT_ONOFF_PRESS:
        return "PANEL_EVENT_ONOFF_PRESS";
    case PANEL_EVENT_ONOFF_PRESS_HOLD:
        return "PANEL_EVENT_ONOFF_PRESS_HOLD";
    case PANEL_EVENT_ONOFF_PRESS_DOUBLE:
        return "PANEL_EVENT_ONOFF_PRESS_DOUBLE";
    case PANEL_EVENT_ONOFF_RELEASE:
        return "PANEL_EVENT_ONOFF_RELEASE";
    case PANEL_EVENT_FUNCTION_PRESS:
        return "PANEL_EVENT_FUNCTION_PRESS";
    case PANEL_EVENT_FUNCTION_PRESS_DOUBLE:
        return "PANEL_EVENT_FUNCTION_PRESS_DOUBLE";
    case PANEL_EVENT_FUNCTION_PRESS_HOLD:
        return "PANEL_EVENT_FUNCTION_PRESS_HOLD";
    case PANEL_EVENT_FUNCTION_RELEASE:
        return "PANEL_EVENT_FUNCTION_RELEASE";
    case PANEL_EVENT_FUNCTION_CW:
        return "PANEL_EVENT_FUNCTION_CW";
    case PANEL_EVENT_FUNCTION_CCW:
        return "PANEL_EVENT_FUNCTION_CCW";
    case PANEL_EVENT_COLOR_ARTIFICIAL_PRESS:
        return "PANEL_EVENT_COLOR_ARTIFICIAL_PRESS";
    case PANEL_EVENT_COLOR_ARTIFICIAL_PRESS_DOUBLE:
        return "PANEL_EVENT_COLOR_ARTIFICIAL_PRESS_DOUBLE";
    case PANEL_EVENT_COLOR_ARTIFICIAL_PRESS_HOLD:
        return "PANEL_EVENT_COLOR_ARTIFICIAL_PRESS_HOLD";
    case PANEL_EVENT_COLOR_ARTIFICIAL_RELEASE:
        return "PANEL_EVENT_COLOR_ARTIFICIAL_RELEASE";
    case PANEL_EVENT_COLOR_NATURAL_PRESS:
        return "PANEL_EVENT_COLOR_NATURAL_PRESS";
    case PANEL_EVENT_COLOR_NATURAL_PRESS_DOUBLE:
        return "PANEL_EVENT_COLOR_NATURAL_PRESS_DOUBLE";
    case PANEL_EVENT_COLOR_NATURAL_PRESS_HOLD:
        return "PANEL_EVENT_COLOR_NATURAL_PRESS_HOLD";
    case PANEL_EVENT_COLOR_NATURAL_RELEASE:
        return "PANEL_EVENT_COLOR_NATURAL_RELEASE";
    case PANEL_EVENT_ZOOM_PRESS:
        return "PANEL_EVENT_ZOOM_PRESS";
    case PANEL_EVENT_ZOOM_PRESS_DOUBLE:
        return "PANEL_EVENT_ZOOM_PRESS_DOUBLE";
    case PANEL_EVENT_ZOOM_PRESS_HOLD:
        return "PANEL_EVENT_ZOOM_PRESS_HOLD";
    case PANEL_EVENT_ZOOM_RELEASE:
        return "PANEL_EVENT_ZOOM_RELEASE";
    case PANEL_EVENT_ZOOM_CW:
        return "PANEL_EVENT_ZOOM_CW";
    case PANEL_EVENT_ZOOM_CCW:
        return "PANEL_EVENT_ZOOM_CCW";
    case PANEL_EVENT_SYSTEM_BOOT:
        return "PANEL_EVENT_SYSTEM_BOOT";
    case PANEL_EVENT_SYSTEM_SHUTDOWN:
        return "PANEL_EVENT_SYSTEM_SHUTDOWN";
    default:
        return "UNKNOWN_EVENT";
    }
}

/** @brief Read panel event data from lvipanel device
 * @return 0 on success, negative value on error
 */
int panel_read_event()
{
    if (ioctl(panel_fd, IOCTL_READ_DATA, &panel_event) < 0)
    {
        perror("Failed to read panel data");
        return -1;
    }
    printf("Panel event received: 0x%02X (%s)\n", (unsigned char)panel_event, panel_event_to_string(panel_event));
    return 0;
}

/** @brief Write system event to lvipanel device
 * @param data The system event to write
 * @return 0 on success, negative value on error
 */
int panel_write_event(char data)
{
    if (ioctl(panel_fd, IOCTL_WRITE_DATA, &data) < 0)
    {
        perror("Failed to write panel data");
        return -1;
    }
    return 0;
}

/** @brief Handle panel events and translate to FPGA commands
 */
void handle_panel_event()
{
    int ret = 0;

    switch (panel_event)
    {
    /* ===== ZOOM CONTROL ===== */
    case PANEL_EVENT_ZOOM_CW:
        printf("Panel: Zoom CW -> FPGA: Zoom In\n");
        ret = lvicam_zoom_in();
        break;

    case PANEL_EVENT_ZOOM_CCW:
        printf("Panel: Zoom CCW -> FPGA: Zoom Out\n");
        ret = lvicam_zoom_out();
        break;

    case PANEL_EVENT_ZOOM_PRESS:
        printf("Panel: Zoom Press -> FPGA: Reset Zoom\n");
        ret = lvicam_zoom_reset();
        break;

    case PANEL_EVENT_ZOOM_PRESS_DOUBLE:
        printf("Panel: Zoom Double Press -> FPGA: Query Zoom Position\n");
        ret = lvicam_get_camera1_zoom_pos_status();
        break;

    /* ===== COLOR CONTROL ===== */
    case PANEL_EVENT_COLOR_NATURAL_PRESS:
        printf("Panel: Natural Color Press -> FPGA: Next Natural Color\n");
        ret = lvicam_natcol_next();
        break;

    case PANEL_EVENT_COLOR_ARTIFICIAL_PRESS:
        printf("Panel: Artificial Color Press -> FPGA: Next Artificial Color\n");
        ret = lvicam_artcol_next();
        break;

    /* ===== FUNCTION CONTROL ===== */
    case PANEL_EVENT_FUNCTION_CW:
        printf("Panel: Function CW -> FPGA: Function Scroll Up\n");
        ret = lvicam_function_scroll_up();
        break;

    case PANEL_EVENT_FUNCTION_CCW:
        printf("Panel: Function CCW -> FPGA: Function Scroll Down\n");
        ret = lvicam_function_scroll_down();
        break;

    case PANEL_EVENT_FUNCTION_PRESS:
        printf("Panel: Function Press -> Toggle GStreamer\n");
        gstreamer_toggle();
        break;

    case PANEL_EVENT_FUNCTION_PRESS_DOUBLE:
        printf("Panel: Function Double Press -> FPGA: Function Next\n");
        ret = lvicam_function_next();
        break;

    /* ===== IDLE - NO ACTION ===== */
    case PANEL_EVENT_IDLE:
        // No action for idle
        break;

    /* ===== UNHANDLED EVENTS ===== */
    default:
        printf("Unhandled panel event: %s\n", panel_event_to_string(panel_event));
        break;
    }

    if (ret < 0)
    {
        printf("Error executing FPGA command for event: %s\n", panel_event_to_string(panel_event));
    }
}

/** @brief This function is called when the camera "seesaw" interrupt is triggered.
 * This is historically used to reset the zoom position when setting the camera to distance viewing mode.
 * @param signum The signal number that triggered the interrupt
 */
void lvicam_interrupt_handler(int signum)
{
    if (signum == SIGIO)
    {
        // When the seesaw interrupt is triggered, read the current GPIO value
        struct lvicam_seesaw_status seesaw_status;
        lvicam_read_seesaw(&seesaw_status);

        if (seesaw_status.value)
        {
            printf("Seesaw interrupt: Resetting zoom\n");
            lvicam_zoom_reset();
        }
    }
    else
    {
        printf("Received unexpected signal %d\n", signum);
    }
}

/** @brief Signal handler for panel events
 * @param signum The signal number
 */
void panel_signal_handler(int signum)
{
    if (signum == DATA_AVAILABLE_SIGNAL)
    {
        // Panel has new data available
        if (panel_read_event() == 0)
        {
            handle_panel_event();
        }
    }
    else if (signum == SIGINT)
    {
        printf("\nReceived SIGINT, exiting...\n");
        keep_running = 0;
    }
}

void print_help()
{
    printf("\n=== LVICAM Panel-Controlled Application ===\n");
    printf("This application receives events from the LVI panel and\n");
    printf("translates them to FPGA commands.\n\n");
    printf("Panel Event Mappings:\n");
    printf("  Zoom CW/CCW           -> FPGA Zoom In/Out\n");
    printf("  Zoom Press            -> Reset Zoom\n");
    printf("  Zoom Double Press     -> Query Zoom Position\n");
    printf("  Natural Color Press   -> Cycle Natural Color\n");
    printf("  Artificial Color      -> Cycle Artificial Color\n");
    printf("  Function CW/CCW       -> Scroll Functions\n");
    printf("  Function Press        -> Toggle GStreamer Video\n");
    printf("  Function Double Press -> Execute Function\n");
    printf("\nPress Ctrl+C to exit\n");
    printf("============================================\n\n");
}

int main()
{
    printf("=== LVICAM Panel Control Application ===\n");

    // Open lvicam device
    fd = open("/dev/lvicam", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open /dev/lvicam");
        return 1;
    }
    printf("Opened /dev/lvicam successfully\n");

    // Set up seesaw interrupt handling for lvicam
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_ASYNC);
    signal(SIGIO, lvicam_interrupt_handler);

    // Open lvipanel device
    panel_fd = open(DEVICE_NAME, O_RDWR);
    if (panel_fd < 0)
    {
        perror("Failed to open /dev/lvipanel");
        close(fd);
        return 1;
    }
    printf("Opened /dev/lvipanel successfully\n");

    // Register signal handlers
    signal(DATA_AVAILABLE_SIGNAL, panel_signal_handler);
    signal(SIGINT, panel_signal_handler);

    // Write initial system event to panel (turn on light)
    panel_write_event(SYSTEM_EVENT_ON);

    print_help();

    // Set initial zoom step
    lvicam_zoom_step_set(10);

    // Optional: Query initial status
    // lvicam_get_fpga_status();
    // lvicam_get_camera_id();
    // lvicam_get_camera1_zoom_pos_status();

    printf("Waiting for panel events...\n");

    // Main loop - wait for signals
    while (keep_running)
    {
        pause(); // Wait for signals (panel events or SIGINT)
    }

    // Cleanup
    printf("Cleaning up...\n");

    // Stop GStreamer if running
    if (gstreamer_pid > 0)
    {
        gstreamer_stop();
    }

    panel_write_event(SYSTEM_EVENT_LIGHT_OFF);
    close(panel_fd);
    close(fd);
    printf("Exited cleanly\n");

    return 0;
}