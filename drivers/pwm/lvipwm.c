#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Max Borglowe");
MODULE_DESCRIPTION("PWM control module using platform_driver");

struct pwm_device_data {
    struct pwm_device *pwm;
    // Add any additional data you need
};

static struct pwm_device_data pwm_data;

#define NETLINK_PWM 29 // Choose a unique Netlink family ID

static struct sock *nl_sock;

static void nl_recv_msg(struct sk_buff *skb)
{
    printk("[%s] call", __func__);

    struct nlmsghdr *nlh;
    int duty_cycle;

    nlh = nlmsg_hdr(skb);

    if (nlh->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh->nlmsg_len)
        return;

    duty_cycle = *(int*)NLMSG_DATA(nlh);

    // Access pwm_data.pwm and set the duty cycle
    pwm_config(pwm_data.pwm, duty_cycle, 255);  // Configure PWM parameters (e.g., period, duty cycle)
    // Enable PWM
    pwm_enable(pwm_data.pwm);

    pr_info("Received duty cycle: %d\n", duty_cycle);
}

struct netlink_kernel_cfg nl_cfg = {
    .input = nl_recv_msg,
};

static int pwm_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;

    pr_info("lvipwm: requesting PWM... ");

        // Request PWM
    pwm_data.pwm = devm_pwm_get(dev, NULL);
    if (IS_ERR(pwm_data.pwm)) {
        dev_err(dev, "Failed to request PWM: %ld\n", PTR_ERR(pwm_data.pwm));
        return PTR_ERR(pwm_data.pwm);
        //pwm_data.pwm = pwm_request();
    }

    pr_info("lvipwm: PWM request OK!");

    // Create a Netlink socket
    pr_info("lvipwm: Creating Netlink socket...\n");

    nl_sock = netlink_kernel_create(&init_net, NETLINK_PWM, &nl_cfg);
    if (!nl_sock) {
        pr_err("lvipwm: Failed to create Netlink socket\n");
        return -ENOMEM;
    }

    pr_info("lvipwm: Netlink socket created!\n");


    printk(KERN_INFO "LVI PWM probed!\n");
    pwm_config(pwm_data.pwm, 127, 255);  // Configure PWM parameters (e.g., period, duty cycle)

    // Enable PWM
    pwm_enable(pwm_data.pwm);

    pr_info("LVI PWM module initialized!\n");

    return 0;
}

static int pwm_remove(struct platform_device *pdev)
{
    printk(KERN_INFO "LVI PWM module being removed...\n");

    // Destroy Netlink socket
    netlink_kernel_release(nl_sock);

    pwm_config(pwm_data.pwm, 0, 255);  // Disable PWM
    pwm_disable(pwm_data.pwm);

    // Free PWM
    pwm_free(pwm_data.pwm);

    pr_info("LVI PWM module removed!\n");

    return 0;
}

static const struct of_device_id pwm_of_match[] = {
    { .compatible = "lvipwm" },
    { },
};
MODULE_DEVICE_TABLE(of, pwm_of_match);

static struct platform_driver lvi_pwm_driver = {
    .driver = {
        .name = "lvipwm",
        .of_match_table = of_match_ptr(pwm_of_match),
    },
    .probe = pwm_probe,
    .remove = pwm_remove,
};

module_platform_driver(lvi_pwm_driver);