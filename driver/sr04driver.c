#include <linux/module.h>
#include <linux/poll.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/current.h>
#include <linux/delay.h>
static int major;   // 主设备号，需要在多个函数中使用，设置为全局变量
static struct class *sr04_class;  // class_create的返回值
static struct gpio_desc *sr04_trig;
static struct gpio_desc *sr04_echo;
static int irq;  // 定义中断号
static DECLARE_WAIT_QUEUE_HEAD(sr04_wait);
static u64 sr04_data_ns = 0; 
static char flag = 0;

/* 环形缓冲区 */
#define BUF_LEN 128
static int g_sr04s[BUF_LEN];
static int r, w;
#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_sr04_buf_empty(void)
{
	return (r == w);
}

static int is_sr04_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_sr04(int sr04)
{
	if (!is_sr04_buf_full())
	{
		g_sr04s[w] = sr04;
		w = NEXT_POS(w);
	}
}

static int get_sr04(void)
{
	int sr04 = 0;
	if (!is_sr04_buf_empty())
	{
		sr04 = g_sr04s[r];
		r = NEXT_POS(r);
	}
	return sr04;
}
/* 4.1 打开设备驱动 */
static int sr04_open(struct inode * node, struct file * file){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}
/* D. 6. 实现中断服务函数*/
static irqreturn_t sr04_isr(int irq, void *dev_id)
{	
    static u64 rising_time = 0;
	u64 time;
    int distance;
	/* 读引脚 */
    int val = gpiod_get_value(sr04_echo);
    /* 上升沿 */
	if (val) 
	{
        /* 1. 开始记录时间 */
		rising_time = ktime_get_ns();
        // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	}
    /* 下降沿 */
	else 
	{	
        if (rising_time == 0)
		{
			printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
            /* 发送10us高电平, 触发echo测量, 测量距离 2cm-450cm */
            // printk("valxxx1 %d\n", gpiod_get_value(sr04_trig));
	        gpiod_set_value(sr04_trig, 1);
            udelay(12);
	        gpiod_set_value(sr04_trig, 0);
            return IRQ_HANDLED;
		}
		/* 计算所用时间 */
        time = ktime_get_ns() - rising_time;
        rising_time = 0;
        put_sr04(time);
		/* 2. 唤醒APP:去同一个链表把APP唤醒 */
		wake_up_interruptible(&sr04_wait);
        // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	}
	return IRQ_HANDLED;
}
/* 4.2 向用户空间写入中数据，与用户的read函数对应 */
static ssize_t sr04_read(struct file *file, char __user *buf, size_t size, loff_t *offset){
    int re;
    int timeout;
    int err;
    int sr04_data;
    int val = gpiod_get_value(sr04_trig);
    // printk("valxxx %d\n", val);
    /* 发送10us高电平, 触发echo测量, 测量距离 2cm-450cm */
    // printk("valxxx1 %d\n", gpiod_get_value(sr04_trig));
	gpiod_set_value(sr04_trig, 1);
    udelay(12);
	gpiod_set_value(sr04_trig, 0);
    // printk("valxxx2 %d\n", gpiod_get_value(sr04_trig));
    // 为irq中断注册了一个sr04_isr中断服务函数，上升沿触发
    // if(flag == 0){
    //     flag = 1;
    irq = gpiod_to_irq(sr04_echo);
    
    err = request_irq(irq, sr04_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "imx6ull_sr04", NULL);
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // }
    /* 等待数据 */
    wait_event_interruptible(sr04_wait, !is_sr04_buf_empty());   // 如果 !is_sr04_buf_empty() 不成立，即 sr04 为空，阻塞等待事件；如果按下sr04产生了中断，运行sr04中断处理函数，唤醒gpio_sr04_wait线程，继续往下运行
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // printk("%s %s line %d timeout %d sr04_data_ns %lld\n", __FILE__, __FUNCTION__, __LINE__, timeout, sr04_data_ns);
    // if (timeout){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    free_irq(irq, NULL);
    sr04_data = get_sr04();
    re = copy_to_user(buf, &sr04_data, 4);  // 将sr04_data拷贝到用户空间的buf中
    sr04_data_ns = 0;
	return 4;
	// }
	// else{
    //     printk("time out!!!\n");
    //     sr04_data_ns = 0;
	// 	return -EAGAIN;
	// }
}

/* 4. 定义file_operations结构体 */
static struct file_operations sr04_fop = {
    .owner      = THIS_MODULE,
    .open       = sr04_open,      // 没有open函数也可以打开设备
    .read      = sr04_read,
};

/* 5.1 实现platform_driver的probe函数 */
// 在这个函数能够获取device的resource，并进行device_create
static int sr04_probe(struct platform_device *dev)  // 通过参数struct platform_device *dev获得device中的resource，去操作硬件
{
    int err;
    int val1, val2;
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* 5.1.1 获取硬件信息 */
    // 在设备树中定义有：sr04-gpios = <...>
    sr04_trig = gpiod_get(&dev->dev, "trig", GPIOD_OUT_LOW);  // 输出引脚
    
	sr04_echo = gpiod_get(&dev->dev, "echo", GPIOD_IN);       // 输入引脚
    val1 = gpiod_get_value(sr04_trig);
    val2 = gpiod_get_value(sr04_echo);
    // printk("sr04_trig %d %d\n", val1, val2);
    // 在获得引脚的时候就已经设置了方向，下面就不需要设置方向了

    /* 5.1.2 设置方向 */
    // gpiod_direction_input(sr04_gpio);  // 将引脚配置为输入引脚
    /* 5.1.3 获取中断号 */    
    
    /* 5.1.4 开中断 */
    
    device_create(sr04_class, NULL, MKDEV(major, 0), NULL, "sr04");
    return 0; 
}

/* 5.2 实现platform_driver的remove函数 */
static int sr04_remove(struct platform_device *dev)
{
    /* 5.2.1 销毁设备 */
    device_destroy(sr04_class, MKDEV(major, 0)); /* /dev/sr040,1,... */
    
    /* C. 5.2.4 销毁gpios */
	gpiod_put(sr04_trig);
	gpiod_put(sr04_echo);
    return 0;
}
/* A. 5.3 实现platform_driver的of_device_id，用于支持设备树 */
static const struct of_device_id sr04_match[] = {
	{ .compatible = "imx6ull, sr04" },
	{},
};

/* 5. 定义platform_driver结构体 */
static struct platform_driver sr04_driver = {
    .probe = sr04_probe,      // 当配对时调用probe函数
    .remove = sr04_remove,    // 当取消设备时调用remove函数
    .driver = {
        .name = "sr04",      // 名字和platform_device配对（设备树下最后才匹配这个，一般来说不用它）
        .of_match_table = sr04_match,  // 定义of_match_table，支持设备树
    },
};

/* 1. 定义入口函数 */
static int __init sr04_init(void){   // __init为段属性，表示入口函数只使用一次，用过之后可以释放
    /* 1.1 注册字符设备驱动 */
    major = register_chrdev(0, "sr04", &sr04_fop);  // 参数为"主设备号（设置为0表示自动分配）、驱动名称、file_operations结构体"，返回值为主设备号，注意major为全局变量，在卸载设备驱动时要用；这个函数仅仅是注册了设备节点，如果想使用，需要通过创建设备节点（主设备号和注册的设备节点相同）
    /* 1.2 创建一个类，提供设备信息 */
    sr04_class = class_create(THIS_MODULE, "sr04_class");  // 参数为THIS_MODULE、类的名称，返回值为一个类（全局变量）；在/sys/class/文件下创建了一个hello_class的子目录
    /* 1.3 注册platform_driver */
    platform_driver_register(&sr04_driver);
    return 0;
}

/* 2. 定义出口函数 */
static void __exit sr04_exit(void){   // __exit在驱动编进内核时才起作用，这时就不需要出口函数了   
    /* 2.1 销毁platform_driver */
    platform_driver_unregister(&sr04_driver);
    /* 2.2 销毁类 */
    class_destroy(sr04_class);
    /* 2.3 卸载设备驱动 */
    unregister_chrdev(major, "sr04");   // 参数为"主设备号、驱动名称"
}

/* 3. 注册入口出口函数 */
module_init(sr04_init);     // 注册入口函数，也可以用init_module的方法注册
module_exit(sr04_exit);

MODULE_LICENSE("GPL");