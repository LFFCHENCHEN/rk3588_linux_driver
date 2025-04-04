#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c-dev.h> // Added to define I2C_RDWR
#include <linux/module.h>  // Ensure this is included for MODULE_DEVICE_TABLE
#include <linux/delay.h>

#define DEVICE_NAME "sh3001_aac"
#define CLASS_NAME "i2c_class"

#define IO_READ 0x00
#define IO_WRITE 0x01

static struct i2c_client *client;
static struct class *i2c_class;
static struct device *i2c_device;
static int major_num;

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
    unsigned char data = 0x00;
    unsigned char write_buf[2];
    unsigned int ker_buf[2];
    struct i2c_msg msgs[2];

    if (copy_from_user(ker_buf, usr_buf, sizeof(ker_buf)))
    {
        return -EFAULT; // Return error if copy_from_user fails
    }

    addr = ker_buf[0];
    switch (cmd)
    {
    case IO_READ:
        msgs[0].addr = client->addr;
        msgs[0].flags = 0;
        msgs[0].len = 1;
        msgs[0].buf = &addr;

        msgs[1].addr = client->addr;
        msgs[1].flags = I2C_M_RD;
        msgs[1].len = 1;
        msgs[1].buf = &data;
        i2c_transfer(client->adapter, msgs, 2);

        ker_buf[1] = data;

        if (copy_to_user(usr_buf, ker_buf, sizeof(ker_buf)))
        {
            return -EFAULT; // Return error if copy_to_user fails
        }
        break;
    case IO_WRITE:
        write_buf[0] = addr;
        write_buf[1] = ker_buf[1];

        msgs[0].addr = client->addr;
        msgs[0].flags = 0; /* 写 */
        msgs[0].len = 2;
        msgs[0].buf = write_buf;

        i2c_transfer(client->adapter, msgs, 1);

        mdelay(20);
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
    printk(KERN_INFO "sh3001 device probed.\n");
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
    return 0;
}

static int m_remove(struct i2c_client *cl)
{
    device_destroy(i2c_class, MKDEV(major_num, 0));
    class_unregister(i2c_class);
    class_destroy(i2c_class);
    unregister_chrdev(major_num, DEVICE_NAME);
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