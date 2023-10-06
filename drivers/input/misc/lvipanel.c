

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <net/sock.h> 
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include "lvipanel_commands.h"

MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION("I2C Polling Driver");
MODULE_LICENSE("GPL");

#define NETLINK_USER 31
struct sock *nl_sk = NULL;
static int user_pid = 0;

#define SYSFS_FILENAME "i2c_data"
#ifdef I2C_DATA_BUFFER_SIZE
static u8 i2c_data_buffer[I2C_DATA_BUFFER_SIZE];
#else
static u8 i2c_data_buffer[1];
#endif

#define DEVICE_NAME "lvipanel"
#define I2C_DEVICE_ADDR 0x49

static struct kobject *i2c_kobj;
static struct i2c_client *lvipanel_client;
static struct task_struct *poll_thread;

// Function prototypes
static ssize_t i2c_data_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t i2c_data_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static int lvipanel_polling_thread(void *data);
static u8 lvipanel_read_byte(struct i2c_client *client);
static int lvipanel_write_byte(struct i2c_client *client, u8 byte);
static int lvipanel_write_byte_array(struct i2c_client *client, u8 *byte_array, size_t size);
static int lvipanel_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int lvipanel_remove(struct i2c_client *client);
static void nl_recv_msg(struct sk_buff *skb);
static void nl_send_data_to_user(char value);

// Netlink configuration
static struct netlink_kernel_cfg nl_cfg = {
    .input = nl_recv_msg,
};

/** @brief Show I2C data in sysfs. */
static ssize_t i2c_data_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    #ifdef I2C_DATA_BUFFER_SIZE
    ssize_t len = 0;

    for (i = 0; i < I2C_DATA_BUFFER_SIZE; i++) {
        len += sprintf(buf + len, "0x%02x ", i2c_data_buffer[i]);
    }

    len += sprintf(buf + len, "\n");
    return len;

    #else
    return snprintf(buf, PAGE_SIZE, "0x%02x\n", i2c_data_buffer[0]);
    #endif
}

/** @brief Store data in sysfs file. */
static ssize_t i2c_data_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int parsed_bytes = 0;

    #ifdef I2C_DATA_BUFFER_SIZE
    int i;
    for (i = 0; i < I2C_DATA_BUFFER_SIZE; i++) {
        int bytes_read = sscanf(buf + parsed_bytes, "0x%hhx", &i2c_data_buffer[i]);
        if (bytes_read <= 0) {
            break;
        }
        parsed_bytes += bytes_read;
    }

    #else
    u8 data;
    parsed_bytes = sscanf(buf, "0x%hhx", &data);
    if (parsed_bytes > 0) {
        i2c_data_buffer[0] = data;
    }
    #endif

    return count;
}

static struct kobj_attribute i2c_data_attribute = __ATTR(i2c_data, 0660, i2c_data_show, i2c_data_store);

/** @brief Polling function executed in the kernel thread. */
static int lvipanel_polling_thread(void *data)
{
    printk("[%s] call", __func__);

    while (!kthread_should_stop()) {
        lvipanel_read_byte(lvipanel_client);

        // Polling interval
        msleep_interruptible(50); // 0.5 second delay
    }

    return 0;
}

/** @brief Reads a byte of i2c data from the LVI Panel. */
u8 lvipanel_read_byte(struct i2c_client *client)
{
    struct i2c_msg msg;
    char read_byte;
    int ret;

    // Prepare the I2C messages for read operation
    msg.addr = I2C_DEVICE_ADDR;
    msg.flags = I2C_M_RD;
    msg.len = 1; // Number of bytes to read
    msg.buf = &read_byte;

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret < 0) {
        printk(KERN_ERR "I2C transfer error: %d\n", ret);
        return ret;
    } else {
    
        if(read_byte != 0x00){
            printk(KERN_INFO "Read data: 0x%x\n", read_byte);
            #ifdef I2C_DATA_BUFFER_SIZE
            memmove(i2c_data_buffer + 1, i2c_data_buffer, I2C_DATA_BUFFER_SIZE - 1);
            #endif
            i2c_data_buffer[0] = read_byte; // Update i2c_data_buffer
            kobject_uevent(i2c_kobj, KOBJ_CHANGE); // Notify sysfs

            nl_send_data_to_user(read_byte);
        }

    }

    return read_byte;
}

/** @brief Receives messages from Netlink. */
static void nl_recv_msg(struct sk_buff *skb)
{
    printk("[%s] call", __func__);

    struct nlmsghdr *nlh;
    u8 value;

    printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

    nlh = (struct nlmsghdr *)skb->data;
    value = *((u8 *)nlmsg_data(nlh));

    printk(KERN_INFO "Netlink received msg payload:%c\n", value);
    user_pid = nlh->nlmsg_pid; /* pid of sending process */

    nlh = nlmsg_hdr(skb);

    if (nlh->nlmsg_len < sizeof(u8)) {
        printk(KERN_WARNING "Invalid Netlink message length\n");
        return;
    }

    if(value != 0x00){
        lvipanel_write_byte(lvipanel_client, value);
        printk(KERN_INFO "Received 0x%x from user space, and written to LVI Panel\n", value);
    }
    
}

/** @brief Sends data to user via Netlink. */
static void nl_send_data_to_user(char value)
{

    printk("[%s] call", __func__);

    if (user_pid != 0) {
        struct sk_buff *skb;
        struct nlmsghdr *nlh;

        skb = nlmsg_new(NLMSG_ALIGN(sizeof(u8)), GFP_KERNEL);
        if (!skb) {
            printk(KERN_ERR "Failed to allocate skb\n");
            return;
        }

        nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, sizeof(u8), 0);
        memcpy(nlmsg_data(nlh), &value, sizeof(u8));

        netlink_unicast(nl_sk, skb, user_pid, MSG_DONTWAIT);

        printk(KERN_INFO "Sent netlink data: 0x%x to user process %d\n", value, user_pid);
    } else {
        printk(KERN_WARNING "User PID not set, skipping netlink send\n");
    }
}


/** @brief Writes a byte of i2c data to the LVI Panel. */
static int lvipanel_write_byte(struct i2c_client *client, u8 byte)
{
    struct i2c_msg msg[1];
    int ret;

    // Prepare the I2C messages for write operation
    msg[0].addr = I2C_DEVICE_ADDR;
    msg[0].flags = 0; // Write flag
    msg[0].len = 1; // Number of bytes to write
    msg[0].buf = &byte;

    ret = i2c_transfer(client->adapter, msg, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to send initial data: %d\n", ret);
        return ret;
    } else {
        printk(KERN_INFO "Write data: 0x%x\n", byte);
    }

    return 0;
}

/** @brief Writes an array of bytes via i2c to the LVI Panel. */
static int lvipanel_write_byte_array(struct i2c_client *client, u8 *byte_array, size_t size){

    size_t i;
    for (i = 0; i < size; i++)
    {
        int ret = lvipanel_write_byte(client, byte_array[i]);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

/** @brief Initialization function when the LVI Panel is connected. */
static int lvipanel_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    /* Initial stuff 
    This part is executed once upon the initial calling of lvipanel_probe */

    printk("[%s] call", __func__);

    lvipanel_client = client;

    i2c_kobj = kobject_create_and_add("i2c_data", NULL);
    if (!i2c_kobj) {
        return -ENOMEM;
    }

    if (sysfs_create_file(i2c_kobj, &i2c_data_attribute.attr)) {
        kobject_put(i2c_kobj);
        return -ENOMEM;
    }
    
    memset(i2c_data_buffer, 0, sizeof(i2c_data_buffer));

    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg,
    };

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT "Error creating socket.\n");
        return -ENOMEM;
    }

    printk(KERN_INFO "LVI Panel initiated");

    /* Initial commands to be written to the LVI Panel.
    The commands are specified in lvi_panel_commands.h */
    u8 init_buff[] =
    {
        COMM_START_SC,
        COMM_LD_OFF,
        COMM_LD_GREENFLASH,
        COMM_LD_GREENFLASH
    };

    size_t buff_size = sizeof(init_buff)/sizeof(init_buff[0]);
    lvipanel_write_byte_array(client, init_buff, buff_size);

    /* End of initial stuff */
    
    /* Create and start the polling thread. */
    poll_thread = kthread_run(lvipanel_polling_thread, NULL, DEVICE_NAME);
    if (IS_ERR(poll_thread)) {
        printk(KERN_ERR "Failed to create polling thread\n");
        return PTR_ERR(poll_thread);
    }

    return 0;
}

/** @brief Cleanup function when the LVI Panel is disconnected. */
static int lvipanel_remove(struct i2c_client *client)
{
    printk("[%s] call", __func__);

    sysfs_remove_file(i2c_kobj, &i2c_data_attribute.attr);
    kobject_put(i2c_kobj);

    /* Commands that shall be sent upon removal of LVI Panel drive module */
    u8 exit_buff[] =
    {
        COMM_LD_OFF,
        COMM_LD_REDFLASH,
        COMM_STOP_SC
    };

    size_t buff_size = sizeof(exit_buff)/sizeof(exit_buff[0]);
    lvipanel_write_byte_array(client, exit_buff, buff_size);

    // Stop the polling thread
    kthread_stop(poll_thread);

    netlink_kernel_release(nl_sk);

    printk(KERN_INFO "LVI Panel removed");

    return 0;
}

static const struct of_device_id __maybe_unused lvipanel_of_match[] = {
    { .compatible = "lvipanel", },
    { },
};
MODULE_DEVICE_TABLE(of, lvipanel_of_match);

static struct i2c_driver lvipanel_driver = {
    .driver = {
        .name = DEVICE_NAME,
        .of_match_table = of_match_ptr(lvipanel_of_match),
    },
    .probe = lvipanel_probe,
    .remove = lvipanel_remove,
};

module_i2c_driver(lvipanel_driver);

