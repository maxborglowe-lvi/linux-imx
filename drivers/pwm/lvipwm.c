#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

#include "../../lib/lviconfig/lviconfig_parameters.h"

#define PWM_MAGIC 'P'

#define PWM_IOCTL_SET_DUTY _IOW(PWM_MAGIC, 0, int)
#define PWM_IOCTL_SET_PERIOD _IOW(PWM_MAGIC, 1, int)
#define PWM_IOCTL_ENABLE _IO(PWM_MAGIC, 2)
#define PWM_IOCTL_DISABLE _IO(PWM_MAGIC, 3)
#define PWM_IOCTL_GET_DUTY _IOR(PWM_MAGIC, 4, int)

#define DEVICE_NAME "lvipwm"

struct lvipwm_drvdata {
	struct pwm_device *pwm;
	struct cdev cdev; // Add this cdev member
};

static int major;
static struct class *pwm_class;
struct pwm_args pargs;
struct pwm_state state;

unsigned char pwm_state_checked = 0;

static long lvipwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lvipwm_drvdata *drvdata = file->private_data;
	struct pwm_device *pwm = drvdata->pwm;
	int value;

	if (!pwm) {
		pr_err("[%s] PWM device is NULL!\n", __func__);
		return -ENODEV;
	}

	pr_info("[%s] IOCTL cmd=0x%x, duty_cycle=%llu, period=%llu, enabled=%d\n", __func__, cmd, (unsigned long long)state.duty_cycle, (unsigned long long)state.period, state.enabled);

	switch (cmd) {
	case PWM_IOCTL_SET_DUTY:
		if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
			pr_err("[%s] copy_from_user failed for SET_DUTY\n", __func__);
			return -EFAULT;
		}
		pr_info("[%s] Setting duty cycle to %d\n", __func__, value);
		state.duty_cycle = value;
		break;

	case PWM_IOCTL_SET_PERIOD:
		if (copy_from_user(&value, (int __user *)arg, sizeof(int))) {
			pr_err("[%s] copy_from_user failed for SET_PERIOD\n", __func__);
			return -EFAULT;
		}
		pr_info("[%s] Setting period to %d\n", __func__, value);
		state.period = value;
		break;
	case PWM_IOCTL_GET_DUTY:
		if (copy_to_user((int __user *)arg, &state.duty_cycle, sizeof(int))) {
			pr_err("[%s] copy_to_user failed for GET_DUTY\n", __func__);
			return -EFAULT;
		}
		pr_info("[%s] Getting duty cycle: %llu\n", __func__, (unsigned long long)state.duty_cycle);
		return 0;

	case PWM_IOCTL_ENABLE:
		pr_info("[%s] Enabling PWM\n", __func__);
		state.enabled = true;
		break;

	case PWM_IOCTL_DISABLE:
		pr_info("[%s] Disabling PWM\n", __func__);
		state.enabled = false;
		break;

	default:
		pr_err("[%s] Invalid IOCTL cmd=0x%x\n", __func__, cmd);
		return -EINVAL;
	}

	return pwm_apply_state(pwm, &state);
}

static int lvipwm_open(struct inode *inode, struct file *file)
{
	struct lvipwm_drvdata *drvdata;

	pr_info("[%s] called\n", __func__);
	drvdata = container_of(inode->i_cdev, struct lvipwm_drvdata, cdev);
	file->private_data = drvdata;

	return 0;
}

static int lvipwm_release(struct inode *inode, struct file *file)
{
	pr_info("[%s] called\n", __func__);
	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = lvipwm_open,
	.release = lvipwm_release,
	.unlocked_ioctl = lvipwm_ioctl,
};

static int lvipwm_probe(struct platform_device *pdev)
{
	struct lvipwm_drvdata *drvdata;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device *device;
	dev_t devno;
	int ret;

	pr_info("[%s] called\n", __func__);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		pr_err("[%s] Failed to allocate drvdata\n", __func__);
		return -ENOMEM;
	}

	drvdata->pwm = of_pwm_get(dev, np, NULL);
	if (IS_ERR(drvdata->pwm)) {
		pr_err("[%s] Unable to get PWM device: %ld\n", __func__, PTR_ERR(drvdata->pwm));
		return PTR_ERR(drvdata->pwm);
	}

	pr_info("[%s] PWM device fetched from device tree\n", __func__);

	ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("[%s] alloc_chrdev_region failed: %d\n", __func__, ret);
		return ret;
	}

	major = MAJOR(devno);

	cdev_init(&drvdata->cdev, &fops);
	drvdata->cdev.owner = THIS_MODULE;

	ret = cdev_add(&drvdata->cdev, devno, 1);
	if (ret < 0) {
		pr_err("[%s] cdev_add failed: %d\n", __func__, ret);
		goto unregister_region;
	}

	pwm_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(pwm_class)) {
		ret = PTR_ERR(pwm_class);
		pr_err("[%s] class_create failed: %d\n", __func__, ret);
		goto del_cdev;
	}

	device = device_create(pwm_class, NULL, devno, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("[%s] device_create failed: %d\n", __func__, ret);
		goto destroy_class;
	}

	pr_info("[%s] Device created: %s\n", __func__, DEVICE_NAME);

	platform_set_drvdata(pdev, drvdata);

	/* Fetch PWM data from DT and apply the duty cycles stored in the configuration (fetched from EEPROM at boot) */
	pwm_get_args(drvdata->pwm, &pargs);
	pwm_get_state(drvdata->pwm, &state);
	state.period = pargs.period;
	state.duty_cycle = confLighting.Intensity.data[0] | (confLighting.Intensity.data[1] << 8);
	state.polarity = pargs.polarity;
	state.enabled = true;

	pr_info("[%s] PWM args set\n", __func__);

	ret = pwm_apply_state(drvdata->pwm, &state);
	if (ret < 0) {
		pr_err("[%s] pwm_apply_state failed: %d\n", __func__, ret);
		goto destroy_class;
	}

	pr_info("[%s] PWM state applied\n", __func__);

	pr_info("[%s] Initializing PWM state: period=%llu, duty_cycle=%llu, enabled=%d\n", __func__, (unsigned long long)state.period, (unsigned long long)state.duty_cycle, state.enabled);

	pr_info("[%s] lvipwm driver probed\n", __func__);
	return 0;

destroy_class:
	class_destroy(pwm_class);
del_cdev:
	cdev_del(&drvdata->cdev);
unregister_region:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static int lvipwm_remove(struct platform_device *pdev)
{
	struct lvipwm_drvdata *drvdata = platform_get_drvdata(pdev);
	dev_t devno = MKDEV(major, 0);

	pr_info("[%s] called\n", __func__);

	pwm_disable(drvdata->pwm);
	pwm_put(drvdata->pwm);

	device_destroy(pwm_class, devno);
	class_destroy(pwm_class);
	cdev_del(&drvdata->cdev);
	unregister_chrdev_region(devno, 1);

	pr_info("[%s] lvipwm driver removed\n", __func__);
	return 0;
}

static const struct of_device_id lvipwm_of_match[] = { {
							       .compatible = "lvi,lvipwm",
						       },
						       { /* sentinel */ } };
MODULE_DEVICE_TABLE(of, lvipwm_of_match);

static struct platform_driver lvipwm_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = lvipwm_of_match,
	},
	.probe = lvipwm_probe,
	.remove = lvipwm_remove,
};

module_platform_driver(lvipwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("PWM driver using platform device model");
