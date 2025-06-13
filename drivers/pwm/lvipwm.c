#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/ioctl.h>

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

static long pwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lvipwm_drvdata *drvdata = file->private_data;
	struct pwm_device *pwm = drvdata->pwm;
	struct pwm_state state;
	int value;

	if (!pwm) {
		pr_err("[%s] PWM device is NULL!\n", __func__);
		return -ENODEV;
	}

	pwm_get_state(pwm, &state);

	switch (cmd) {
	case PWM_IOCTL_SET_DUTY:
		if (copy_from_user(&value, (int __user *)arg, sizeof(int)))
			return -EFAULT;
		state.duty_cycle = value;
		break;

	case PWM_IOCTL_SET_PERIOD:
		if (copy_from_user(&value, (int __user *)arg, sizeof(int)))
			return -EFAULT;
		state.period = value;
		break;
	case PWM_IOCTL_GET_DUTY:
		if (copy_to_user((int __user *)arg, &state.duty_cycle,
				 sizeof(int)))
			return -EFAULT;
		return 0;

	case PWM_IOCTL_ENABLE:
		state.enabled = true;
		break;

	case PWM_IOCTL_DISABLE:
		state.enabled = false;
		break;

	default:
		return -EINVAL;
	}

	return pwm_apply_state(pwm, &state);
}

static int pwm_open(struct inode *inode, struct file *file)
{
	struct lvipwm_drvdata *drvdata;

	drvdata = container_of(inode->i_cdev, struct lvipwm_drvdata, cdev);
	file->private_data = drvdata;

	return 0;
}

static int pwm_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = pwm_open,
	.release = pwm_release,
	.unlocked_ioctl = pwm_ioctl,
};

static int lvipwm_probe(struct platform_device *pdev)
{
	struct lvipwm_drvdata *drvdata;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device *device;
	dev_t devno;
	int ret;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->pwm = of_pwm_get(dev, np, NULL);
	if (IS_ERR(drvdata->pwm)) {
		dev_err(dev, "Unable to get PWM device\n");
		return PTR_ERR(drvdata->pwm);
	}

	ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
	if (ret < 0)
		return ret;

	major = MAJOR(devno);

	cdev_init(&drvdata->cdev, &fops);
	drvdata->cdev.owner = THIS_MODULE;

	ret = cdev_add(&drvdata->cdev, devno, 1);
	if (ret < 0)
		goto unregister_region;

	pwm_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(pwm_class)) {
		ret = PTR_ERR(pwm_class);
		goto del_cdev;
	}

	device = device_create(pwm_class, NULL, devno, NULL, DEVICE_NAME);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		goto destroy_class;
	}

	platform_set_drvdata(pdev, drvdata);
	dev_info(dev, "lvipwm driver probed\n");
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

	pwm_disable(drvdata->pwm);
	pwm_put(drvdata->pwm);

	device_destroy(pwm_class, devno);
	class_destroy(pwm_class);
	cdev_del(&drvdata->cdev);
	unregister_chrdev_region(devno, 1);

	dev_info(&pdev->dev, "lvipwm driver removed\n");
	return 0;
}

static const struct of_device_id lvipwm_of_match[] = {
	{
		.compatible = "lvi,lvipwm",
	},
	{ /* sentinel */ }
};
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
