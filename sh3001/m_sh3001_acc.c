#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c-dev.h> // Added to define I2C_RDWR
#include <linux/module.h>  // Ensure this is included for MODULE_DEVICE_TABLE
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/workqueue.h> // Include workqueue header

#define DEVICE_NAME "sh3001_aac"
#define CLASS_NAME "i2c_class"

#define IO_READ 0x00
#define IO_WRITE 0x01

static struct i2c_client *client;
static struct class *i2c_class;
static struct device *i2c_device;

static struct input_dev *input_dev;
static struct timer_list m_timer;
static struct workqueue_struct *i2c_workqueue;
static struct work_struct i2c_work;
static int major_num;

int i2c_read_bytes(unsigned char addr, unsigned int len, unsigned char *des)
{
    unsigned char address = addr;
    struct i2c_msg msgs[2];
    int ret;

    msgs[0].flags = !I2C_M_RD;   // 写
    msgs[0].addr = client->addr; // 器件地址
    msgs[0].len = 1;
    msgs[0].buf = &address;

    msgs[1].flags = I2C_M_RD;    // 读
    msgs[1].addr = client->addr; // 器件地址
    msgs[1].len = len;           // 读取的数据长度
    msgs[1].buf = des;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret == 2)
        return 0;
    else
        return -1;
}

static int i2c_write_bytes(unsigned char addr, unsigned len, unsigned char *buf)
{
    struct i2c_msg msg;
    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = len;
    msg.buf = buf;

    i2c_transfer(client->adapter, &msg, 1);

    return 0;
}

static void i2c_work_func(struct work_struct *work)
{
    u8 data[6] = {0x00};
    short x, y, z;

    // Perform I2C read operation
    i2c_read_bytes(0x00, 6, data);

    // Combine high and low bytes
    x = (data[1] << 8) | data[0];
    y = (data[3] << 8) | data[2];
    z = (data[5] << 8) | data[4];

    // Report the accelerometer data
    input_report_abs(input_dev, ABS_X, x);
    input_report_abs(input_dev, ABS_Y, y);
    input_report_abs(input_dev, ABS_Z, z);
    input_sync(input_dev);

    // printk("x:%02x y:%02x z:%02x", x, y, z);
}

static void m_timer_func(struct timer_list *t)
{
    // Queue the work instead of performing I2C read directly
    queue_work(i2c_workqueue, &i2c_work);

    // Reset timer for next reading (100ms)
    mod_timer(&m_timer, jiffies + msecs_to_jiffies(1000));
}

static int m_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "sh3001 device opened.\n");
    return 0;
}

static ssize_t m_read(struct file *file, char *buf, size_t len, loff_t *pos)
{
    return 0;
}

static long m_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned long *usr_buf = (unsigned long *)arg;
    unsigned char addr;
    unsigned int ker_buf[2];

    if (copy_from_user(ker_buf, usr_buf, sizeof(ker_buf)))
    {
        return -EFAULT; // Return error if copy_from_user fails
    }
    addr = ker_buf[0];

    switch (cmd)
    {
    case IO_READ:
        i2c_read_bytes(addr, 1, (unsigned char *)&ker_buf[1]);
        if (copy_to_user(usr_buf, ker_buf, sizeof(ker_buf)))
        {
            return -EFAULT; // Return error if copy_to_user fails
        }
        break;
    case IO_WRITE:
        i2c_write_bytes(addr, 1, (unsigned char *)&ker_buf[1]);
        break;
    default:
        break;
    }
    return 0;
}
static struct file_operations fops = {
    // Corrected from file_operation to file_operations
    .open = m_open,
    .read = m_read,
    .unlocked_ioctl = m_ioctl,
};

static int m_probe(struct i2c_client *cli, const struct i2c_device_id *id)
{
    // 创建sh3001设备
    if (!i2c_check_functionality(cli->adapter, I2C_FUNC_I2C))
    {
        pr_err("I2C_FUNC_I2C not supported.\n");
        return -ENODEV;
    }
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0)
    {
        pr_err("Failed to register character device.\n");
        return major_num;
    }

    i2c_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(i2c_class))
    {
        unregister_chrdev(major_num, DEVICE_NAME);
        pr_err("Failed to create class.\n");
        return PTR_ERR(i2c_class);
    }

    i2c_device = device_create(i2c_class, NULL, MKDEV(major_num, 0), NULL, DEVICE_NAME);
    if (IS_ERR(i2c_device))
    {
        class_destroy(i2c_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        pr_err("Failed to create device.\n");
        return PTR_ERR(i2c_device);
    }
    client = cli;
    printk(KERN_INFO "Device node created successfully: /dev/%s\n", DEVICE_NAME);
    // 初始化input输入设备
    input_dev = input_allocate_device();
    if (!input_dev)
    {
        device_destroy(i2c_class, MKDEV(major_num, 0));
        class_destroy(i2c_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        pr_err("Failed to allocate input device.\n");
        return -ENOMEM;
    }

    input_dev->name = "sh3001_accelerometer";
    input_dev->id.bustype = BUS_I2C;
    set_bit(EV_ABS, input_dev->evbit);
    input_set_abs_params(input_dev, ABS_X, -32768, 32767, 0, 0);
    input_set_abs_params(input_dev, ABS_Y, -32768, 32767, 0, 0);
    input_set_abs_params(input_dev, ABS_Z, -32768, 32767, 0, 0);

    if (input_register_device(input_dev))
    {
        input_free_device(input_dev);
        device_destroy(i2c_class, MKDEV(major_num, 0));
        class_destroy(i2c_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        pr_err("Failed to register input device.\n");
        return -ENODEV;
    }

    // Initialize workqueue
    i2c_workqueue = create_singlethread_workqueue("i2c_workqueue");
    if (!i2c_workqueue)
    {
        pr_err("Failed to create workqueue.\n");
        return -ENOMEM;
    }
    INIT_WORK(&i2c_work, i2c_work_func);

    // Initialize timer
    timer_setup(&m_timer, m_timer_func, 0);
    mod_timer(&m_timer, jiffies + msecs_to_jiffies(1000));

    printk(KERN_INFO "sh3001 device probe ok.\n");
    return 0;
}

static int m_remove(struct i2c_client *cl)
{
    del_timer_sync(&m_timer);
    input_unregister_device(input_dev);
    device_destroy(i2c_class, MKDEV(major_num, 0));
    class_unregister(i2c_class);
    class_destroy(i2c_class);
    unregister_chrdev(major_num, DEVICE_NAME);

    // Destroy workqueue
    if (i2c_workqueue)
    {
        flush_workqueue(i2c_workqueue);
        destroy_workqueue(i2c_workqueue);
    }

    return 0;
}

static const struct i2c_device_id m_i2c_id[] = {
    {"m_i2c_adv", 0},
    {}};
MODULE_DEVICE_TABLE(i2c, m_i2c_id); // Ensure this is in the global scope

static const struct of_device_id m_iic_of_match[] = {
    {.compatible = "sh3001_acc"},
    {},
};
MODULE_DEVICE_TABLE(of, m_iic_of_match);

static struct i2c_driver m_driver = {
    .driver = {.name = "sh3001aacDrv", .owner = THIS_MODULE, .of_match_table = m_iic_of_match},
    .probe = m_probe,
    .remove = m_remove,
    .id_table = m_i2c_id,
};

module_i2c_driver(m_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cc");
MODULE_DESCRIPTION("Advanced I2C Driver with i2c_transfer");