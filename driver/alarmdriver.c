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
static int major;   // 主设备号，需要在多个函数中使用，设置为全局变量
static struct class *alarm_class;  // class_create的返回值
static struct gpio_desc *alarm_gpio;
/* 4.1 打开设备驱动 */
static int alarm_open(struct inode * node, struct file * file){
    /* D. 4.1.1 初始化alarm*/
    // gpiod_direction_input(alarm_gpio);  // 将引脚配置为输入引脚
    return 0;
}

/* 4.2 向用户空间写入中数据，与用户的read函数对应 */
static ssize_t alarm_write(struct file *file, const char __user *buf, size_t size, loff_t *offset){
    int re;
    unsigned char ker_buf[1];
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // len = (size < 4) ? size : 4;   // 计算需要拷贝的长度
    // extern inline long copy_to_user(void __user *to, const void *from, long n)
    re = copy_from_user(ker_buf, buf, size);  // 将kernel_buf拷贝到用户空间的buf中
    gpiod_set_value(alarm_gpio, ker_buf[0]);
    return 1;
}

/* 4. 定义file_operations结构体 */
static struct file_operations alarm_fop = {
    .owner      = THIS_MODULE,
    .open       = alarm_open,      // 没有open函数也可以打开设备
    .write      = alarm_write,
};

/* 5.1 实现platform_driver的probe函数 */
// 在这个函数能够获取device的resource，并进行device_create
static int alarm_probe(struct platform_device *dev)  // 通过参数struct platform_device *dev获得device中的resource，去操作硬件
{
    // printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    /* C.5.1.1 获取alarm-gpios*/
    // 从dev->dev设备里获得名为alarm的引脚，第三个参数可以指定引脚的方向
    alarm_gpio = gpiod_get(&dev->dev, "alarm", GPIOD_OUT_LOW);  // 在设备树中定义有：alarm-gpios = <...>
    /* 5.1.4 创建一个设备 */
    device_create(alarm_class, NULL, MKDEV(major, 0), NULL, "alarm");  // 参数为：创建的class、NULL、设备号(主, 次）、NULL、设备名称（/dev/alarm 0 ,1,...)
    return 0;
}
/* 5.2 实现platform_driver的remove函数 */
static int alarm_remove(struct platform_device *dev)
{
    /* 5.2.1 销毁设备 */
    device_destroy(alarm_class, MKDEV(major, 0)); /* /dev/alarm0,1,... */
    /* C. 5.2.4 销毁gpios */
	gpiod_put(alarm_gpio);
    return 0;
}
/* A. 5.3 实现platform_driver的of_device_id，用于支持设备树 */
static const struct of_device_id alarm_match[] = {
	{ .compatible = "imx6ull, alarm" },
	{},
};

/* 5. 定义platform_driver结构体 */
static struct platform_driver alarm_driver = {
    .probe = alarm_probe,      // 当配对时调用probe函数
    .remove = alarm_remove,    // 当取消设备时调用remove函数
    .driver = {
        .name = "alarm",      // 名字和platform_device配对（设备树下最后才匹配这个，一般来说不用它）
        .of_match_table = alarm_match,  // 定义of_match_table，支持设备树
    },
};

/* 1. 定义入口函数 */
static int __init alarm_init(void){   // __init为段属性，表示入口函数只使用一次，用过之后可以释放
    /* 1.1 注册字符设备驱动 */
    major = register_chrdev(0, "alarm", &alarm_fop);  // 参数为"主设备号（设置为0表示自动分配）、驱动名称、file_operations结构体"，返回值为主设备号，注意major为全局变量，在卸载设备驱动时要用；这个函数仅仅是注册了设备节点，如果想使用，需要通过创建设备节点（主设备号和注册的设备节点相同）
    /* 1.2 创建一个类，提供设备信息 */
    alarm_class = class_create(THIS_MODULE, "alarm_class");  // 参数为THIS_MODULE、类的名称，返回值为一个类（全局变量）；在/sys/class/文件下创建了一个hello_class的子目录
    /* 1.3 注册platform_driver */
    platform_driver_register(&alarm_driver);
    return 0;
}

/* 2. 定义出口函数 */
static void __exit alarm_exit(void){   // __exit在驱动编进内核时才起作用，这时就不需要出口函数了   
    /* 2.1 销毁platform_driver */
    platform_driver_unregister(&alarm_driver);
    /* 2.2 销毁类 */
    class_destroy(alarm_class);
    /* 2.3 卸载设备驱动 */
    unregister_chrdev(major, "alarm");   // 参数为"主设备号、驱动名称"
}

/* 3. 注册入口出口函数 */
module_init(alarm_init);     // 注册入口函数，也可以用init_module的方法注册
module_exit(alarm_exit);

MODULE_LICENSE("GPL");