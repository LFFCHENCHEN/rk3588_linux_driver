#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h> // Include this header for of_get_named_gpio
#include <asm/uaccess.h>
#include <asm/io.h>

#define GPIOLED_CNT 1
#define GPIOLED_NAME "ofgpioled"
#define LEDOFF 0
#define LEDON 1

struct ofgpioled_dev
{
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    int major;
    int minor;
    struct device_node *nd;
    int led_gpio;
};

struct ofgpioled_dev ofgpioled;

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &ofgpioled;
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct ofgpioled_dev *dev = filp->private_data; // Fix the typo here
    ret = copy_from_user(databuf, buf, cnt);
    if (ret < 0)
    {
        printk("kernel wtite failed.\r\n");
        return -EFAULT;
    }
    ledstat = databuf[0];
    if (ledstat == LEDON)
    {
        gpio_set_value(dev->led_gpio, 1);
    }
    else if (ledstat == LEDOFF)
    {
        gpio_set_value(dev->led_gpio, 0);
    }

    return 0;
}

static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations ofgpioled_fops =
    {
        .owner = THIS_MODULE,
        .open = led_open,
        .read = led_read,
        .write = led_write,
        .release = led_release,
};

static int __init
led_init(void)
{
    int ret = 0;
    const char *str;
    ofgpioled.nd = of_find_node_by_path("/gpioled");
    if (ofgpioled.nd == NULL)
    {
        printk("ofgpioled node not find.\r\n");
        return -EINVAL;
    }
    ret = of_property_read_string(ofgpioled.nd, "status", &str);
    if (ret < 0)
    {
        return -EINVAL;
    }
    if (strcmp(str, "okay"))
    {
        return -EINVAL;
    }
    ret = of_property_read_string(ofgpioled.nd, "compatible", &str);
    if (ret < 0)
    {
        printk("ofgpioled comaptible not find.\r\n");
        return -EINVAL;
    }
    if (strcmp(str, "alientek,led"))
    {
        printk("ofgpioled comaptible match faied.\r\n");
        return -EINVAL;
    }
    ofgpioled.led_gpio = of_get_named_gpio(ofgpioled.nd, "led-gpio", 0);
    if (ofgpioled.led_gpio < 0)
    {
        printk("ofgpioled get gpio faied.\r\n");
        return -EINVAL;
    }
    printk("led-gpio num %d .\r\n", ofgpioled.led_gpio);

    ret = gpio_request(ofgpioled.led_gpio, "LED-GPIO");
    if (ret)
    {
        printk(KERN_ERR "ofgpioled:failed to req gpio.\r\n");
        return -EINVAL;
    }
    ret = gpio_direction_output(ofgpioled.led_gpio, 0);
    if (ret < 0)
    {
        printk("failed to set dir.\r\n");
    }
    if (ofgpioled.major)
    {
        ofgpioled.devid = MKDEV(ofgpioled.major, 0);
        ret = register_chrdev_region(ofgpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        if (ret < 0)
        {
            pr_err("cannot register %s char driver [ret=%d]\n", GPIOLED_NAME, GPIOLED_CNT);
            goto free_gpio;
        }
    }
    else
    { /* 没有定义设备号 */
        ret = alloc_chrdev_region(&ofgpioled.devid, 0, GPIOLED_CNT,
                                  GPIOLED_NAME); /* 申请设备号 */
        if (ret < 0)
        {
            pr_err("%s Couldn't alloc_chrdev_region,ret=%d\r\n",
                   GPIOLED_NAME, ret);
            goto free_gpio;
        }
        ofgpioled.major = MAJOR(ofgpioled.devid); /* 获取分配号的主设备号*/
        ofgpioled.minor = MINOR(ofgpioled.devid); /* 获取分配号的次设备号*/
    }
    printk("gpioled major=%d,minor=%d\r\n", ofgpioled.major, ofgpioled.minor);
    ofgpioled.cdev.owner = THIS_MODULE;
    cdev_init(&ofgpioled.cdev, &ofgpioled_fops);

    /* 3、添加一个 cdev */
    ret = cdev_add(&ofgpioled.cdev, ofgpioled.devid, GPIOLED_CNT);
    if (ret < 0)
        goto del_unregister;
    ofgpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(ofgpioled.class))
    {
        goto del_cdev;
    }

    /* 5、创建设备 */
    ofgpioled.device = device_create(ofgpioled.class, NULL,
                                     ofgpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(ofgpioled.device))
    {
        goto destroy_class;
    }
    return 0;

destroy_class:
    class_destroy(ofgpioled.class);
del_cdev:
    cdev_del(&ofgpioled.cdev);
del_unregister:
    unregister_chrdev_region(ofgpioled.devid, GPIOLED_CNT);
free_gpio:
    gpio_free(ofgpioled.led_gpio);
    return -EIO;
}

static void __exit led_exit(void)
{
    /* 注销字符设备驱动 */
    cdev_del(&ofgpioled.cdev); /* 删除 cdev */
    unregister_chrdev_region(ofgpioled.devid, GPIOLED_CNT);
    device_destroy(ofgpioled.class, ofgpioled.devid); /* 注销设备 */
    class_destroy(ofgpioled.class);                   /* 注销类 */
    gpio_free(ofgpioled.led_gpio);                    /* 释放 GPIO */
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");