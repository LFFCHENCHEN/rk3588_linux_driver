#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/wait.h>
#include <linux/sched.h>

static struct proc_dir_entry *proc_entry;
static DECLARE_WAIT_QUEUE_HEAD(m_wait_queue);
static int condition = 0;

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    printk(KERN_INFO "proc_read called\n");
    if (wait_event_interruptible(m_wait_queue, condition))
    {
        return -ERESTARTSYS;
    }
    condition = 0;
    return simple_read_from_buffer(buf, count, pos, "WAKEUP.\n", 8);
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    char cmd;
    if (copy_from_user(&cmd, buf, sizeof(cmd)))
    {
        return -EFAULT;
    }
    if (cmd == '1')
    {
        condition = 1;
        wake_up_interruptible(&m_wait_queue);
        printk(KERN_INFO "wake_up_interruptible called\n");
    }
    return count;
}
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init m_wait_queue_init(void)
{
    proc_entry = proc_create("m_wait_queue", 0666, NULL, &proc_fops);
    if (!proc_entry)
    {
        printk(KERN_ERR "Failed to create proc entry\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "m_wait_queue module loaded\n");
    return 0;
}

static void __exit m_wait_queue_exit(void)
{
    proc_remove(proc_entry);
    condition = 1;
    wake_up_interruptible(&m_wait_queue);
    printk(KERN_INFO "m_wait_queue module unloaded\n");
}

module_init(m_wait_queue_init);
module_exit(m_wait_queue_exit);
MODULE_LICENSE("GPL");