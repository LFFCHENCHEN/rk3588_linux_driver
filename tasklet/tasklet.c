#include <linux/module.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

static struct proc_dir_entry *proc_entry;

static void m_tasklet_func(struct tasklet_struct *tasklet)
{
    // 这里是 tasklet 的处理函数
    printk(KERN_INFO "Tasklet execute on cpu %d\n", smp_processor_id());
}

static DECLARE_TASKLET(m_tasklet, m_tasklet_func);

static ssize_t proc_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    char val;
    if (copy_from_user(&val, buf, sizeof(val)))
    {
        return -EFAULT;
    }
    if (val == '1')
    {
        tasklet_schedule(&m_tasklet);
    }
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_write = proc_write,
};

static int __init m_tasklet_init(void)
{
    proc_entry = proc_create("m_tasklet", 0666, NULL, &proc_fops);
    if (!proc_entry)
    {
        printk(KERN_ERR "Failed to create proc entry\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "m_tasklet module loaded\n");
    return 0;
}

static void __exit m_tasklet_exit(void)
{
    tasklet_kill(&m_tasklet);
    proc_remove(proc_entry);
    printk(KERN_INFO "m_tasklet module unloaded\n");
}
module_init(m_tasklet_init);
module_exit(m_tasklet_exit);
MODULE_LICENSE("GPL");