/*
 * cat5157_dpot.c - I2C driver for the CAT5157 digital potentiometer
 *
 * This driver reads the resistance range and default value from device tree,
 * maps the resistance to an 8-bit code (0-255), and writes that code to the
 * device via I²C. It also creates a sysfs attribute "resistance" (in ohms)
 * to view or change the value.
 *
 * (C) 2025 Your Name
 * Licensed under the GPL v2
 */

 #include <linux/module.h>
 #include <linux/i2c.h>
 #include <linux/of.h>
 #include <linux/slab.h>
 #include <linux/sysfs.h>
 #include <linux/mutex.h>
 
 #define CAT5157_MAX_CODE 255
 
 struct cat5157_data {
	 struct i2c_client *client;
	 struct mutex lock;       /* protects access to value */
	 u8 value;                /* current digital code */
	 u32 resistance_min;      /* in ohms */
	 u32 resistance_max;      /* in ohms */
	 u32 resistance_default;  /* in ohms */
 };
 
 /*
  * Convert an ohm value to the corresponding code.
  * The conversion is linear between resistance_min and resistance_max.
  */
 static u8 resistance_to_code(u32 resistance, u32 rmin, u32 rmax)
 {
	 u32 code;
 
	 /* Clamp the value */
	 if (resistance < rmin)
		 resistance = rmin;
	 if (resistance > rmax)
		 resistance = rmax;
 
	 /* Scale: code = (resistance - rmin) * 255 / (rmax - rmin) */
	 code = (resistance - rmin) * CAT5157_MAX_CODE + ((rmax - rmin) / 2);
	 code /= (rmax - rmin);
 
	 return (u8)code;
 }
 
 /*
  * Convert the stored digital code back to an ohm value.
  */
 static u32 code_to_resistance(u8 code, u32 rmin, u32 rmax)
 {
	 u32 resistance;
 
	 resistance = rmin + (u32)code * (rmax - rmin) / CAT5157_MAX_CODE;
	 return resistance;
 }
 
 /*
  * Write the given code value to the device.
  * (Assumes a one-byte write sets the potentiometer value.)
  */
 static int cat5157_set_value(struct cat5157_data *cat, u8 value)
 {
	 int ret;
 
	 /* Log that this function is called */
	 printk(KERN_INFO "[%s] call\n", __func__);
 
	 ret = i2c_smbus_write_byte(cat->client, value);
	 if (ret < 0) {
		 dev_err(&cat->client->dev, "Failed to write value (0x%02x): %d\n",
			 value, ret);
		 return ret;
	 }
 
	 cat->value = value;
	 dev_info(&cat->client->dev, "Wrote value 0x%02x to device\n", value);
	 return 0;
 }
 
 /*
  * Sysfs "resistance" attribute: read in ohms.
  */
 static ssize_t resistance_show(struct device *dev,
					struct device_attribute *attr, char *buf)
 {
	 struct cat5157_data *cat = dev_get_drvdata(dev);
	 u32 resistance;
 
	 /* Log that this function is called */
	 printk(KERN_INFO "[%s] call\n", __func__);
 
	 mutex_lock(&cat->lock);
	 resistance = code_to_resistance(cat->value, cat->resistance_min, cat->resistance_max);
	 mutex_unlock(&cat->lock);
	 dev_dbg(dev, "Reading resistance: %u ohms (code 0x%02x)\n",
		 resistance, cat->value);
	 return scnprintf(buf, PAGE_SIZE, "%u\n", resistance);
 }
 
 /*
  * Sysfs "resistance" attribute: write new value (in ohms).
  */
 static ssize_t resistance_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
 {
	 struct cat5157_data *cat = dev_get_drvdata(dev);
	 unsigned long resistance;
	 u8 code;
	 int ret;
 
	 /* Log that this function is called */
	 printk(KERN_INFO "[%s] call\n", __func__);
 
	 ret = kstrtoul(buf, 10, &resistance);
	 if (ret)
		 return ret;
 
	 if (resistance < cat->resistance_min || resistance > cat->resistance_max)
		 return -EINVAL;
 
	 code = resistance_to_code(resistance, cat->resistance_min, cat->resistance_max);
 
	 mutex_lock(&cat->lock);
	 ret = cat5157_set_value(cat, code);
	 mutex_unlock(&cat->lock);
	 if (ret)
		 return ret;
 
	 dev_info(dev, "Updated resistance to %lu ohms (code 0x%02x)\n",
		  resistance, code);
	 return count;
 }
 static DEVICE_ATTR_RW(resistance);
 
 static struct attribute *cat5157_attrs[] = {
	 &dev_attr_resistance.attr,
	 NULL,
 };
 
 static const struct attribute_group cat5157_attr_group = {
	 .attrs = cat5157_attrs,
 };
 
 static int cat5157_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
 {
	 struct cat5157_data *cat;
	 int ret;
	 u32 tmp;
 
	 /* Log that this function is called */
	 printk(KERN_INFO "[%s] call\n", __func__);
	 dev_info(&client->dev, "Probing CAT5157 digital potentiometer driver\n");
 
	 cat = devm_kzalloc(&client->dev, sizeof(*cat), GFP_KERNEL);
	 if (!cat)
		 return -ENOMEM;
 
	 cat->client = client;
	 mutex_init(&cat->lock);
 
	 /* Read DT properties */
	 if (client->dev.of_node) {
		 ret = of_property_read_u32(client->dev.of_node,
						"resistance-min", &cat->resistance_min);
		 if (ret) {
			 dev_err(&client->dev, "Missing resistance-min property\n");
			 return ret;
		 }
		 dev_info(&client->dev, "Resistance min: %u ohm\n", cat->resistance_min);
 
		 ret = of_property_read_u32(client->dev.of_node,
						"resistance-max", &cat->resistance_max);
		 if (ret) {
			 dev_err(&client->dev, "Missing resistance-max property\n");
			 return ret;
		 }
		 dev_info(&client->dev, "Resistance max: %u ohm\n", cat->resistance_max);
 
		 ret = of_property_read_u32(client->dev.of_node,
						"resistance-default", &cat->resistance_default);
		 if (ret) {
			 dev_warn(&client->dev,
				  "Missing resistance-default property, using min value\n");
			 cat->resistance_default = cat->resistance_min;
		 }
		 dev_info(&client->dev, "Default resistance: %u ohm\n",
			  cat->resistance_default);
	 } else {
		 dev_err(&client->dev, "No device tree node found\n");
		 return -EINVAL;
	 }
 
	 /* Validate the range */
	 if (cat->resistance_min >= cat->resistance_max) {
		 dev_err(&client->dev, "Invalid resistance range: min (%u) >= max (%u)\n",
			 cat->resistance_min, cat->resistance_max);
		 return -EINVAL;
	 }
 
	 /* Set the default value */
	 tmp = cat->resistance_default;
	 if (tmp < cat->resistance_min || tmp > cat->resistance_max) {
		 dev_warn(&client->dev, "Default resistance %u out of range, clamping\n",
			  tmp);
		 if (tmp < cat->resistance_min)
			 tmp = cat->resistance_min;
		 else
			 tmp = cat->resistance_max;
	 }
	 cat->value = resistance_to_code(tmp, cat->resistance_min, cat->resistance_max);
 
	 dev_info(&client->dev, "Calculated default code: 0x%02x for resistance %u ohm\n",
		  cat->value, tmp);
 
	 ret = cat5157_set_value(cat, cat->value);
	 if (ret)
		 return ret;
 
	 i2c_set_clientdata(client, cat);
 
	 ret = sysfs_create_group(&client->dev.kobj, &cat5157_attr_group);
	 if (ret) {
		 dev_err(&client->dev, "Failed to create sysfs group\n");
		 return ret;
	 }
 
	 dev_info(&client->dev, "CAT5157 digital potentiometer driver successfully probed\n");
	 return 0;
 }
 
 static int cat5157_remove(struct i2c_client *client)
 {
	 /* Log that this function is called */
	 printk(KERN_INFO "[%s] call\n", __func__);
	 sysfs_remove_group(&client->dev.kobj, &cat5157_attr_group);
	 dev_info(&client->dev, "CAT5157 digital potentiometer driver removed\n");
	 return 0;
 }
 
 static const struct of_device_id cat5157_of_match[] = {
	 { .compatible = "cat,cat5157", },
	 { /* sentinel */ }
 };
 MODULE_DEVICE_TABLE(of, cat5157_of_match);
 
 static const struct i2c_device_id cat5157_id[] = {
	 { "cat5157", 0 },
	 { }
 };
 MODULE_DEVICE_TABLE(i2c, cat5157_id);
 
 static struct i2c_driver cat5157_driver = {
	 .driver = {
		 .name = "cat5157_dpot",
		 .of_match_table = of_match_ptr(cat5157_of_match),
	 },
	 .probe    = cat5157_probe,
	 .remove   = cat5157_remove,
	 .id_table = cat5157_id,
 };
 
 module_i2c_driver(cat5157_driver);
 
 MODULE_AUTHOR("Max Borglowe");
 MODULE_DESCRIPTION("I2C driver for CAT5157 digital potentiometer");
 MODULE_LICENSE("GPL");
 