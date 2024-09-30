#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

// GPIO pin number (modify according to your hardware)
#define GPIO_LVI_SEESAW 0x50 /* GPIO3_IO16 --> GPIOn_IOx number: (n - 1) * 32 + x --> (3-1) * 32 + 16 = 80 = 0x50 */

static struct gpio_desc *gpio_desc;

static irqreturn_t gpio_interrupt_handler(int irq, void *dev_id)
{
    pr_info("GPIO interrupt triggered!\n");
    return IRQ_HANDLED;
}

static int gpio_module_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev->of_node;
    int irq;

    if (!np)
    {
        pr_err("No device tree node found\n");
        return -ENODEV;
    }

    // Get the GPIO pin from the device tree
    gpio_desc = gpio_to_desc(GPIO_LVI_SEESAW);
    if (!gpio_desc || IS_ERR(gpio_desc))
    {
        pr_err("Failed to get GPIO pin from device tree\n");
        return PTR_ERR(gpio_desc);
    }

    unsigned long flags = gpiod_get_value(gpio_desc);

    // Request the GPIO pin
    if (gpiod_direction_input(gpio_desc))
    {
        pr_err("Failed to set GPIO direction to input\n");
        gpiod_put(gpio_desc);
        return -EINVAL;
    }

    // Request an interrupt line for the GPIO pin
    irq = gpiod_to_irq(gpio_desc);
    if (irq < 0)
    {
        pr_err("Failed to get IRQ for GPIO pin\n");
        gpiod_put(gpio_desc);
        return irq;
    }

    // Set up the interrupt handler
    if (request_irq(irq, gpio_interrupt_handler, IRQF_TRIGGER_FALLING, "lviseesaw", NULL))
    {
        pr_err("Failed to request IRQ\n");
        gpiod_put(gpio_desc);
        return -EINVAL;
    }

    pr_info("GPIO interrupt module initialized\n");

    return 0;
}

static int gpio_module_remove(struct platform_device *pdev)
{
    int irq = gpiod_to_irq(gpio_desc);

    // Release the interrupt line
    free_irq(irq, NULL);

    // Release the GPIO pin
    if (gpio_desc)
        gpiod_put(gpio_desc);

    pr_info("GPIO interrupt module removed\n");

    return 0;
}

static const struct of_device_id gpio_module_of_match[] = {
    { .compatible = "lviseesaw", },
    {},
};
MODULE_DEVICE_TABLE(of, gpio_module_of_match);

static struct platform_driver gpio_module_driver = {
    .driver = {
        .name = "lviseesaw",
        .of_match_table = gpio_module_of_match,
    },
    .probe = gpio_module_probe,
    .remove = gpio_module_remove,
};

module_platform_driver(gpio_module_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A GPIO interrupt module");
