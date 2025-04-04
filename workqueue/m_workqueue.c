#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/delay.h> // Added to resolve msleep implicit declaration error

static struct proc_dir_entry *proc_entry;
static struct workqueue_struct *m_wq;
static struct work_struct m_work;

static void m_work_func(struct work_struct *work)
{
    printk(KERN_INFO "Workqueue execute on cpu %d\n", smp_processor_id());
    msleep(50); // 模拟耗时操作(tasklet 不能使用 msleep)
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
    char val;
    if (copy_from_user(&val, buf, sizeof(val)))
    {
        return -EFAULT;
    }
    if (val == '1')
    {
        queue_work(m_wq, &m_work);
        printk(KERN_INFO "Workqueue scheduled\n");
    }
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_write = proc_write,
};

static int __init m_wq_init(void)
{
    m_wq=alloc_workqueue("m_wq", WQ_MEM_RECLAIM, 1);
    if (!m_wq)
    {
        printk(KERN_ERR "Failed to create workqueue\n");
        return -ENOMEM;
    }
    INIT_WORK(&m_work, m_work_func);
    proc_entry = proc_create("m_wq", 0666, NULL, &proc_fops);
    if (!proc_entry)
    {
        destroy_workqueue(m_wq);
        printk(KERN_ERR "Failed to create proc entry\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "m_wq module loaded\n");
    return 0;
}

static void __exit m_wq_exit(void)
{
    flush_workqueue(m_wq);
    destroy_workqueue(m_wq);
    proc_remove(proc_entry);
    printk(KERN_INFO "m_wq module unloaded\n");
}
module_init(m_wq_init);
module_exit(m_wq_exit);
MODULE_LICENSE("GPL");


