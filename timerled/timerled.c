#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/platform_device.h> // Include this header for struct platform_device
#include <linux/timer.h>
struct platformled
{
    int major;
    struct class *platformled_class;
    struct gpio_desc *led_gpio;
    struct timer_list timer;
};

static struct platformled platformled;
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &platformled;
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char ledstat;
    struct platformled *dev = filp->private_data; // Fix the typo here
    ret = copy_from_user(databuf, buf, cnt);
    if (ret < 0)
    {
        printk("kernel wtite failed.\r\n");
        return -EFAULT;
    }
    ledstat = databuf[0];
    if (ledstat == 1)
    {
        gpiod_set_value(dev->led_gpio, 1);
    }
    else if (ledstat == 0)
    {
        gpiod_set_value(dev->led_gpio, 0);
    }
    return 0;
}
static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}
static struct file_operations platformled_fops =
    {
        .owner = THIS_MODULE,
        .open = led_open,
        .write = led_write,
        .release = led_release,
};

void timer_function(struct timer_list *t)
{
    struct platformled *dev = from_timer(dev, t, timer);
    static int ledstat = 0;
    ledstat = !ledstat; // Toggle the LED state
    gpiod_set_value(dev->led_gpio, ledstat);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(1000)); // 1 second interval
}

// 修改后的 probe 函数
static int platformled_probe(struct platform_device *pdev)
{
    int ret;
    // 关键修改点：指定 con_id 为 "led"
    platformled.led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(platformled.led_gpio))
    {
        pr_err("failed to get gpio\n");
        return PTR_ERR(platformled.led_gpio);
    }

    // 设置 GPIO 方向（可省略，因 GPIOD_OUT_HIGH 已隐含）
    ret = gpiod_direction_output(platformled.led_gpio, 0);
    if (ret < 0)
    {
        pr_err("failed to set dir.\r\n");
        return ret; // 无需手动 put，devm 已管理
    }

    device_create(platformled.platformled_class, NULL,
                  MKDEV(platformled.major, 0), NULL, "platformled");
    // Start the timer
    add_timer(&platformled.timer);
    return 0;
}

static int platformled_remove(struct platform_device *pdev)
{
    device_destroy(platformled.platformled_class, MKDEV(platformled.major, 0));
    // gpiod_put(platformled.led_gpio);
    return 0;
}

static const struct of_device_id platformled_of_match[] = {
    {.compatible = "alientek,led"},
    {},
};

static struct platform_device_id platformled_id_table[] = {
    {.name = "platformled"},
    {},
};
static struct platform_driver platformled_driver = {
    .driver = {
        .name = "platformled",
        .owner = THIS_MODULE,
        .of_match_table = platformled_of_match,
    },
    .probe = platformled_probe,
    .remove = platformled_remove,
    .id_table = platformled_id_table,
}; // Add the missing semicolon here

MODULE_DEVICE_TABLE(of, platformled_of_match);

static int __init platformledInit(void) // Replace _init with __init
{
    int ret;
    // Initialize the timer
    timer_setup(&platformled.timer, timer_function, 0);
    platformled.timer.expires = jiffies + msecs_to_jiffies(1000); // 1 second interval
    // platformled.timer.function = timer_function;
    // platformled.timer.data = (unsigned long)&platformled; // Pass the platformled struct to the timer
    platformled.major = register_chrdev(0, "platformled", &platformled_fops);
    if (platformled.major < 0)
    {
        pr_err("failed to register char device\n");
        return platformled.major;
    }
    platformled.platformled_class = class_create(THIS_MODULE, "platformled_class");
    if (IS_ERR(platformled.platformled_class))
    {
        pr_err("failed to create class\n");
        unregister_chrdev(platformled.major, "platformled");
        return PTR_ERR(platformled.platformled_class);
    }
    ret = platform_driver_register(&platformled_driver);
    if (ret)
    {
        pr_err("failed to register platform driver\n");
        class_destroy(platformled.platformled_class);
        unregister_chrdev(platformled.major, "platformled");
        return ret;
    }

    return 0;
}

static void __exit platformledExit(void) // Replace _exit with __exit
{
    platform_driver_unregister(&platformled_driver);
    class_destroy(platformled.platformled_class);
    unregister_chrdev(platformled.major, "platformled");
    gpio_set_value(platformled.led_gpio, 0); // Turn off the LED
    del_timer_sync(&platformled.timer); // Stop the timer
}
module_init(platformledInit);
module_exit(platformledExit);
MODULE_LICENSE("GPL");
