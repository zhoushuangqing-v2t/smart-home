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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/kernel.h>
static int major;   // 主设备号，需要在多个函数中使用，设置为全局变量
static struct class *irda_class;  // class_create的返回值
static struct gpio_desc *irda_gpio;
static int irq;  // 定义中断号
static DECLARE_WAIT_QUEUE_HEAD(irda_wait);
static u64 irda_time[68];   // 记录每个中断时间
static int irda_count = 0;
unsigned int irda_num;
static struct timer_list irda_timer;
static int flagData = 0;
/* 环形缓冲区 */
#define BUF_LEN 128
static unsigned char g_irdas[BUF_LEN];
static int r, w;
#define NEXT_POS(x) ((x+1) % BUF_LEN)

static int is_irda_buf_empty(void)
{
	return (r == w);
}

static int is_irda_buf_full(void)
{
	return (r == NEXT_POS(w));
}

static void put_irda(unsigned char irda)
{
	if (!is_irda_buf_full())
	{
		g_irdas[w] = irda;
		w = NEXT_POS(w);
	}
}

static char get_irda(void)
{
	char irda = 0;
	if (!is_irda_buf_empty())
	{
		irda = g_irdas[r];
		r = NEXT_POS(r);
	}
	return irda;
}
void irda_datas(void){
    int i;
    // u64 low_time;
    u64 high_time;
    unsigned char data = 0;
    unsigned char datas[4];
    int bits = 0;
    int byte = 0;
    // // 出错了
    // if(irda_count < 81){
    //     put_irda(-1);
    //     put_irda(-1);
    //     irda_count = 0;
    //     wake_up(&irda_wait);
    //     return;
    // }
    /* 1. 判断前导码：9ms低脉冲，4.5ms高脉冲 */
    // low_time = irda_time[1] - irda_time[0];
    // if(low_time < 8000000 || low_time > 10000000){
    //     put_irda(-1);
    //     put_irda(-1);
    //     printk("%d line err! %lld\n", __LINE__, low_time);
    // }

    // high_time = irda_time[2] - irda_time[1];
    // if(high_time < 3500000 || high_time > 5500000){ 
    //     put_irda(-1);
    //     put_irda(-1);
    //     printk("%d line err! %lld\n", __LINE__, high_time);
    // }
    
    /* 2. 解析数据 */
    for(i = 4; i < 67; i += 2){
        high_time = irda_time[i] - irda_time[i - 1];
        // low_time = irda_time[i - 1] - irda_time[i - 2];
        data <<= 1;  // 左移一位
        if(high_time > 1000000){  /* data 1*/
            data |= 1;
        }
        bits++;
        if(bits == 8){
            datas[byte++] = data;
            data = 0;
            bits = 0;
        }
    }
    /* 3. 判断数据的有效性 */
	datas[1] = ~datas[1];
	datas[3] = ~datas[3];
    if((datas[0] != datas[1]) || (datas[2] != datas[3])){ 
        put_irda(-1);
        put_irda(-1);
        printk("%d line err!\n", __LINE__);
    }else{
        put_irda(datas[0]);   // 放入环形缓冲区
        put_irda(datas[2]);
    }
    // wake_up_interruptible(&irda_wait);        
}
static int irda_repeat(void){
    u64 time;
    /* 判断重复码 : 9ms的低脉冲, 2.25ms高脉冲  */
    time = irda_time[1] - irda_time[0];
    if(time < 8000000 || time > 10000000){
        return 0;
    }

    time = irda_time[2] - irda_time[1];
    if(time < 2000000 || time > 2500000){
        return 0;
    }
    return 1;   // 是重复码，返回1
}

static int irda_forward(void){
    u64 time;
    /* 1. 判断前导码：9ms低脉冲，4.5ms高脉冲 */
    time = irda_time[1] - irda_time[0];
    if(time < 8000000 || time > 10000000){
        return 0;
    }

    time = irda_time[2] - irda_time[1];
    if(time < 3500000 || time > 5500000){ 
        return 0;
    }
    return 1;   // 是重复码，返回1
}
/* D. 6. 实现中断服务函数*/
static irqreturn_t irda_isr(int irq, void *dev_id)
{
	u64 time;
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* 1. 记录中断发生的时间 */
    time = ktime_get_ns();
    irda_time[irda_count] = time;
    /* 2. 累加次数 */
    irda_count++;
    // if(irda_count == 2){
    //     printk("irda_count %lld %lld\n", irda_time[0], irda_time[1]);
    // }
    /* 判断是否是重复码 */
    if(irda_count == 4){
        if(irda_repeat()){
            printk("repeat!!\n");
        	/* device: 0, val: 0, 表示重复码 */
			// put_irda(0);
			// put_irda(0);
            // wake_up_interruptible(&irda_wait); 
            // del_timer(&irda_timer);
            irda_count = 0;
            del_timer(&irda_timer);   // 删除定时器
            // mod_timer(&irda_timer, jiffies + msecs_to_jiffies(100));
            return IRQ_HANDLED;
        }else if(!irda_forward()){
            printk("irda_forward err!!\n");
            irda_count = 0;
            del_timer(&irda_timer);   // 删除定时器
            return IRQ_HANDLED;
        }
    }
    /* 3. 次数足够，解析数据，放入环形缓冲区，唤醒APP*/
    if(irda_count == 68){
        irda_datas();
        flagData = 1;
        del_timer(&irda_timer);   // 删除定时器
        irda_count = 0; 
        return IRQ_HANDLED;   // 删除定时器后马上返回，不再重新启动定时器了
    }
    /* 4. 启动定时器 */
    mod_timer(&irda_timer, jiffies + msecs_to_jiffies(500));   // 定时器超时，表示出错
	return IRQ_HANDLED;
}

static void irda_timer_expire(unsigned long data)
{
	// 解析数据, 放入环形buffer, 唤醒APP
    irda_count = 0;
    put_irda(-1);
    put_irda(-1);
    // wake_up_interruptible(&irda_wait);
    printk("time err!!!\n");
}
/* 4.1 打开设备驱动 */
static int irda_open(struct inode * node, struct file * file){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

/* 4.2 向用户空间写入中数据，与用户的read函数对应 */
static ssize_t irda_read(struct file *file, char __user *buf, size_t size, loff_t *offset){
    int re;
    char ker_buf[2];
    if(size != 2){
        printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
        return -EINVAL;
    }
    // 休眠等待
    // printk("wait %d %d %d\n", is_irda_buf_empty(), r, w);
    // wait_event_interruptible(irda_wait, !is_irda_buf_empty());   // 如果 !is_irda_buf_empty() 不成立，即 irda 为空，阻塞等待事件；如果按下irda产生了中断，运行irda中断处理函数，唤醒gpio_irda_wait线程，继续往下运行
    if(is_irda_buf_empty() == 0){
        flagData = 0;
        ker_buf[0] = get_irda();   /* 设备 */
	    ker_buf[1] = get_irda();   /* 数据 */
        // printk("ker_buf 0x%02x, 0x%02x\n", ker_buf[0], ker_buf[1]);
        if(ker_buf[0] == (unsigned char)(-1) && ker_buf[1] == (unsigned char)(-1)){
            printk("ker_buf err!\n");
            ker_buf[0] = -1;
            ker_buf[1] = -1;
        }
    }else{
        ker_buf[0] = -1;
        ker_buf[1] = -1;
        // printk("ker_buf1 0x%02x, 0x%02x\n", ker_buf[0], ker_buf[1]);
    }
    
    re = copy_to_user(buf, ker_buf, 2);  // 将irda_data拷贝到用户空间的buf中
    return 2;
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // /* 1. 发送18us的低脉冲 */
	// gpiod_set_value(irda_gpio, 0);
	// mdelay(18);
    // /* 设置为输入引脚 */
    // gpio_direction_input(irda_num);  /* 引脚变为输入方向, 由上拉电阻拉为1 */
    // /* 2. 注册中断 */
    // err = request_irq(irq, irda_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "imx6ull_irda", NULL);
    // if(err){
    //     printk("irq err\n");
    // }
    // setup_timer(&irda_timer, irda_timer_expire, 0);
    // mod_timer(&irda_timer, jiffies + 10);
    // /* 3. 休眠等待数据 */
    // /* 5. 释放irq */
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // free_irq(irq, NULL);

    // printk("get val %d, %d\n", ker_buf[0], ker_buf[1]);

    // /* 4. 拷贝到用户空间 */
    
    // gpio_direction_output(irda_num, 1);
    
}

/* 4. 定义file_operations结构体 */
static struct file_operations irda_fop = {
    .owner      = THIS_MODULE,
    .open       = irda_open,      // 没有open函数也可以打开设备
    .read      = irda_read,
};

/* 5.1 实现platform_driver的probe函数 */
// 在这个函数能够获取device的resource，并进行device_create
static int irda_probe(struct platform_device *dev)  // 通过参数struct platform_device *dev获得device中的resource，去操作硬件
{
    int err;
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* 5.1.1 获取硬件信息 */
    // 在设备树中定义有：irda-gpios = <...>
    irda_gpio = gpiod_get(&dev->dev, "irda", GPIOD_IN);  // 先配置为输出引脚
    // irda_num = desc_to_gpio(irda_gpio);
    // setup_timer(&irda_timer, irda_timer_expire, 0);
    setup_timer(&irda_timer, irda_timer_expire, 0);
    // 在获得引脚的时候就已经设置了方向，下面就不需要设置方向了
    /* 5.1.3 获取中断号 */    
    irq = gpiod_to_irq(irda_gpio);
    /* 5.1.4 开中断 */
    // 为irq中断注册了一个irda_isr中断服务函数，上升沿触发
    err = request_irq(irq, irda_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "imx6ull_irda", NULL);
    device_create(irda_class, NULL, MKDEV(major, 0), NULL, "myirda");
    return 0; 
}
/* 5.2 实现platform_driver的remove函数 */
static int irda_remove(struct platform_device *dev)
{
    /* 5.2.1 销毁设备 */
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    device_destroy(irda_class, MKDEV(major, 0)); /* /dev/irda0,1,... */
    free_irq(irq, NULL);
    del_timer(&irda_timer);
    /* C. 5.2.4 销毁gpios */
	gpiod_put(irda_gpio);
    return 0;
}
/* A. 5.3 实现platform_driver的of_device_id，用于支持设备树 */
static const struct of_device_id irda_match[] = {
	{ .compatible = "imx6ull, irda" },
	{},
};

/* 5. 定义platform_driver结构体 */
static struct platform_driver irda_driver = {
    .probe = irda_probe,      // 当配对时调用probe函数
    .remove = irda_remove,    // 当取消设备时调用remove函数
    .driver = {
        .name = "irda",      // 名字和platform_device配对（设备树下最后才匹配这个，一般来说不用它）
        .of_match_table = irda_match,  // 定义of_match_table，支持设备树
    },
};

/* 1. 定义入口函数 */
static int __init irda_init(void){   // __init为段属性，表示入口函数只使用一次，用过之后可以释放
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    /* 1.1 注册字符设备驱动 */
    major = register_chrdev(0, "irda", &irda_fop);  // 参数为"主设备号（设置为0表示自动分配）、驱动名称、file_operations结构体"，返回值为主设备号，注意major为全局变量，在卸载设备驱动时要用；这个函数仅仅是注册了设备节点，如果想使用，需要通过创建设备节点（主设备号和注册的设备节点相同）
    /* 1.2 创建一个类，提供设备信息 */
    irda_class = class_create(THIS_MODULE, "irda_class");  // 参数为THIS_MODULE、类的名称，返回值为一个类（全局变量）；在/sys/class/文件下创建了一个hello_class的子目录
    /* 1.3 注册platform_driver */
    platform_driver_register(&irda_driver);
    return 0;
}

/* 2. 定义出口函数 */
static void __exit irda_exit(void){   // __exit在驱动编进内核时才起作用，这时就不需要出口函数了   
    /* 2.1 销毁platform_driver */
    platform_driver_unregister(&irda_driver);
    /* 2.2 销毁类 */
    class_destroy(irda_class);
    /* 2.3 卸载设备驱动 */
    unregister_chrdev(major, "irda");   // 参数为"主设备号、驱动名称"
}

/* 3. 注册入口出口函数 */
module_init(irda_init);     // 注册入口函数，也可以用init_module的方法注册
module_exit(irda_exit);

MODULE_LICENSE("GPL");