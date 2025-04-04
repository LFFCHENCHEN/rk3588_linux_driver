#include <linux/init.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/timer.h>

static struct input_dev *input_device;
static struct timer_list m_timer;
static int x_dir = 1;
static int y_dir = 2;

static void m_timer_func(struct timer_list *t)
{
    input_report_rel(input_device, REL_X, x_dir++);
    input_report_rel(input_device, REL_Y, y_dir++);
    input_report_key(input_device, BTN_LEFT, 1);
    input_report_key(input_device, BTN_LEFT, 1);
    input_report_key(input_device, BTN_RIGHT, 0);
    input_sync(input_device);
    mod_timer(&m_timer, jiffies + msecs_to_jiffies(500));
}

static int __init m_input_init(void)
{
    input_device = input_allocate_device();
    if (!input_device)
    {
        pr_err("Failed to allocate input device\n");
        return -ENOMEM;
    }
    input_device->name = "m_input_device";
    input_device->id.bustype = BUS_VIRTUAL;

    __set_bit(EV_REL, input_device->evbit); // 相对坐标事件
    __set_bit(REL_X, input_device->relbit); // X轴相对坐标
    __set_bit(REL_Y, input_device->relbit); // Y轴相对坐标
    __set_bit(EV_KEY, input_device->evbit); // 键盘事件
    __set_bit(BTN_LEFT, input_device->keybit); // 左键
    __set_bit(BTN_RIGHT, input_device->keybit); // 右键

    if (input_register_device(input_device))
    {
        pr_err("Failed to register input device\n");
        input_free_device(input_device);
        return -EINVAL;
    }
    timer_setup(&m_timer, m_timer_func, 0);
    mod_timer(&m_timer, jiffies + msecs_to_jiffies(1000));
    pr_info("m_input module loaded\n");
    return 0;
}


static void __exit m_input_exit(void)
{
    del_timer_sync(&m_timer);
    input_unregister_device(input_device);
    pr_info("m_input module unloaded\n");
}
module_init(m_input_init);
module_exit(m_input_exit);
MODULE_LICENSE("GPL");
