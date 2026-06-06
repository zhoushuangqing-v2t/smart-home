#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>  // 引入内核定时器头文件

static int major;   // 主设备号，需要在多个函数中使用，设置为全局变量
static struct class *sr501_class;  // class_create的返回值
static struct gpio_desc *sr501_gpio;
static struct gpio_desc *test_gpio;
static int irq;  // 定义中断号
static DECLARE_WAIT_QUEUE_HEAD(sr501_wait);
static struct timer_list sr501_timer;  // 定义定时器
static struct timer_list sr501_isr_timer;  // 定义定时器
static atomic_t time_exceeded = ATOMIC_INIT(0);          // 标志是否超过10秒
// static char time_exceeded = 0;
/* 4.1 打开设备驱动 */
static int sr501_open(struct inode * node, struct file * file){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

/* 4.2 向用户空间写入中数据，与用户的read函数对应 */
static ssize_t sr501_read(struct file *file, char __user *buf, size_t size, loff_t *offset){
    int re;
    int status;
    char data;
    // static int i;
    // if(i == 0){
    //     gpiod_set_value(sr501_gpio, 0);
    //     i = 1;
    //     printk("%s %s line %d %d\n", __FILE__, __FUNCTION__, __LINE__, gpiod_get_value(sr501_gpio));
    // }else{
    //     gpiod_set_value(sr501_gpio, 1);
    //     i = 0;
    //     printk("%s %s line %d %d\n", __FILE__, __FUNCTION__, __LINE__, gpiod_get_value(sr501_gpio));
    // }
    status = atomic_read(&time_exceeded);
    // wait_event_interruptible(sr501_wait, time_exceeded);
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    if(status == 1){
        data = 1;
        re = copy_to_user(buf, &data, 1);
        // gpiod_set_value(sr501_gpio, 0);
    }else{
        data = 0;
        re = copy_to_user(buf, &data, 1);
    }
    atomic_set(&time_exceeded, 0);
    // // sr501_val = gpiod_get_value(sr501_gpio);
    // // len = (size < 4) ? size : 4;   // 计算需要拷贝的长度
    // // // extern inline long copy_to_user(void __user *to, const void *from, long n)
    // // re = copy_to_user(buf, &sr501_val, len);  // 将kernel_buf拷贝到用户空间的buf中
    // // return len;
    // /* 1. 记录数据*/
    // /* 2. 无数据就休眠：放入某个链表*/
    // /* 等待有数据或者时间超标 */
    // wait_event_interruptible(sr501_wait, time_exceeded);   // 如果 !is_key_buf_empty() 不成立，即 key 为空，阻塞等待事件；如果按下key产生了中断，运行key中断处理函数，唤醒gpio_key_wait线程，继续往下运行
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // // sr501_val = gpiod_get_value(sr501_gpio);  // 获取gpio的值
    // // 这里拷贝的是一个状态量，有动静变为1
    // /* 拷贝状态量给用户空间 */
    // if (time_exceeded) {
    //     data = 1;
    //     re = copy_to_user(buf, &data, 1);  // 将"1"表示超时信息拷贝到用户空间
    //     time_exceeded = 0;  // 重置超时标志
    // } else {
    //     data = 0;
    //     re = copy_to_user(buf, &data, 1);  // 在极端情况下，确保不会出现错误信息
    // }
    // sr501_data = 0;      // 重置数据标志
    return 1;
}

/* 4. 定义file_operations结构体 */
static struct file_operations sr501_fop = {
    .owner      = THIS_MODULE,
    .open       = sr501_open,      // 没有open函数也可以打开设备
    .read      = sr501_read,
};

// 定时器回调函数
static void sr501_timer_callback(unsigned long data) {
    // 有人在超过10s
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    atomic_set(&time_exceeded, 1);  // 标志位设置为1，表示超时
}

void sr501_isr_timer_callback(unsigned long data){
    int value = gpiod_get_value(sr501_gpio);
    // 简单延时消抖，10ms延时
    // msleep(10);
    // printk("%s %s line %d %d %d\n", __FILE__, __FUNCTION__, __LINE__, gpiod_get_value(sr501_gpio), value);
    if(value == 1){   // 上升沿，检测到有人
        // 启动定时器
        // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
        mod_timer(&sr501_timer, jiffies + msecs_to_jiffies(5000));  // 5秒定时器
        // gpiod_set_value(test_gpio, 1);
    }else{   // 人离开
        // 停止并重置定时器标志位
        // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
        del_timer(&sr501_timer);
        // gpiod_set_value(test_gpio, 0);
        // atomic_set(&time_exceeded, 0);  // 重置标志位
    }
}
/* D. 6. 实现中断服务函数*/
static irqreturn_t sr501_isr(int irq, void *dev_id)
{
	// int value = gpiod_get_value(sr501_gpio);
    // // 简单延时消抖，10ms延时
    // // msleep(10);
    // printk("%s %s line %d %d %d\n", __FILE__, __FUNCTION__, __LINE__, gpiod_get_value(sr501_gpio), value);
    // if(value == 1){   // 上升沿，检测到有人
    //     // 启动定时器
    //     // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    //     mod_timer(&sr501_timer, jiffies + msecs_to_jiffies(5000));  // 10秒定时器
    //     // gpiod_set_value(test_gpio, 1);
    // }else{   // 人离开
    //     // 停止并重置定时器标志位
    //     // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    //     del_timer(&sr501_timer);
    //     // gpiod_set_value(test_gpio, 0);
    //     // atomic_set(&time_exceeded, 0);  // 重置标志位
    // }
    mod_timer(&sr501_isr_timer, jiffies + msecs_to_jiffies(10)); // 10ms消抖时间
	return IRQ_HANDLED;
}

/* 5.1 实现platform_driver的probe函数 */
// 在这个函数能够获取device的resource，并进行device_create
static int sr501_probe(struct platform_device *dev)  // 通过参数struct platform_device *dev获得device中的resource，去操作硬件
{
    int err;
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* 5.1.1 获取硬件信息 */
    sr501_gpio = gpiod_get(&dev->dev, "sr501", GPIOD_IN);  // 在设备树中定义有：sr501-gpios = <...>
    // test_gpio = gpiod_get(&dev->dev, "test", GPIOD_OUT_LOW);
    /* 5.1.2 设置方向 */
    // gpiod_direction_input(sr501_gpio);  // 将引脚配置为输入引脚
    /* 5.1.3 获取中断号 */    
    irq = gpiod_to_irq(sr501_gpio);
    /* 5.1.4 开中断 */
    // 为irq中断注册了一个sr501_isr中断服务函数，上升沿触发  #  | IRQF_TRIGGER_FALLING  IRQF_TRIGGER_RISING
    err = request_irq(irq, sr501_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "imx6ull_sr501", NULL);
    /* 初始化定时器 */
    setup_timer(&sr501_timer, sr501_timer_callback, 0);
    setup_timer(&sr501_isr_timer, sr501_isr_timer_callback, 0);
    device_create(sr501_class, NULL, MKDEV(major, 0), NULL, "mysr501");
    return 0; 
}

/* 5.2 实现platform_driver的remove函数 */
static int sr501_remove(struct platform_device *dev)
{
    /* 5.2.1 销毁设备 */
    device_destroy(sr501_class, MKDEV(major, 0)); /* /dev/sr5010,1,... */
    free_irq(irq, NULL);
    /* 删除定时器 */
    del_timer(&sr501_timer);
    /* C. 5.2.4 销毁gpios */
	gpiod_put(sr501_gpio);
    // gpiod_put(test_gpio);
    return 0;
}
/* A. 5.3 实现platform_driver的of_device_id，用于支持设备树 */
static const struct of_device_id sr501_match[] = {
	{ .compatible = "imx6ull, sr501" },
	{},
};

/* 5. 定义platform_driver结构体 */
static struct platform_driver sr501_driver = {
    .probe = sr501_probe,      // 当配对时调用probe函数
    .remove = sr501_remove,    // 当取消设备时调用remove函数
    .driver = {
        .name = "sr501",      // 名字和platform_device配对（设备树下最后才匹配这个，一般来说不用它）
        .of_match_table = sr501_match,  // 定义of_match_table，支持设备树
    },
};

/* 1. 定义入口函数 */
static int __init sr501_init(void){   // __init为段属性，表示入口函数只使用一次，用过之后可以释放
    /* 1.1 注册字符设备驱动 */
    major = register_chrdev(0, "sr501", &sr501_fop);  // 参数为"主设备号（设置为0表示自动分配）、驱动名称、file_operations结构体"，返回值为主设备号，注意major为全局变量，在卸载设备驱动时要用；这个函数仅仅是注册了设备节点，如果想使用，需要通过创建设备节点（主设备号和注册的设备节点相同）
    /* 1.2 创建一个类，提供设备信息 */
    sr501_class = class_create(THIS_MODULE, "sr501_class");  // 参数为THIS_MODULE、类的名称，返回值为一个类（全局变量）；在/sys/class/文件下创建了一个hello_class的子目录
    /* 1.3 注册platform_driver */
    platform_driver_register(&sr501_driver);
    return 0;
}

/* 2. 定义出口函数 */
static void __exit sr501_exit(void){   // __exit在驱动编进内核时才起作用，这时就不需要出口函数了   
    /* 2.1 销毁platform_driver */
    platform_driver_unregister(&sr501_driver);
    /* 2.2 销毁类 */
    class_destroy(sr501_class);
    /* 2.3 卸载设备驱动 */
    unregister_chrdev(major, "sr501");   // 参数为"主设备号、驱动名称"
}

/* 3. 注册入口出口函数 */
module_init(sr501_init);     // 注册入口函数，也可以用init_module的方法注册
module_exit(sr501_exit);

MODULE_LICENSE("GPL");