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
#include <linux/ktime.h>
static int major;   // 主设备号，需要在多个函数中使用，设置为全局变量
static struct class *dht11_class;  // class_create的返回值
static struct gpio_desc *dht11_gpio;
int us_low_array[40];
int us_low_index;
/* 4.1 打开设备驱动 */
static int dht11_open(struct inode * node, struct file * file){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}
static int dht11_wait_for_ready(void)
{
	int timeout_us = 20000;

	/* 等待低电平 */
	while (gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}
	if (!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}

	/* 现在是低电平 */
	/* 等待高电平 */
	timeout_us = 200;
	
	while (!gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}
	
	if (!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}

	/* 现在是高电平 */
	/* 等待低电平 */
	timeout_us = 200;
	while (gpiod_get_value(dht11_gpio) && --timeout_us)
	{
		udelay(1);
	}
	if (!timeout_us)
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -1;
	}
	
	return 0;	
}
static int dht11_read_byte(unsigned char *buf)
{
	int i;
	int us = 0;
	unsigned char data = 0;
	int timeout_us = 200;
	
	for (i = 0; i <8; i++)
	{
		/* 现在是低电平 */
		/* 等待高电平 */
		timeout_us = 400;
		us = 0;
		while (!gpiod_get_value(dht11_gpio) && --timeout_us)
		{
			udelay(1);
			us++;
		}
		if (!timeout_us)
		{
			printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
			return -1;
		}
		us_low_array[us_low_index++] = us;

		/* 现在是高电平 */
		/* 等待低电平,累加高电平的时间 */
		timeout_us = 20000000;
		us = 0;
		
		//last2 = ktime_get_boot_ns();
		//for (j = 0; j < 23; j++)
		udelay(40);

		if (gpiod_get_value(dht11_gpio))
		{
			/* get bit 1 */
			data = (data << 1) | 1;

			/* 当前位的高电平未结束, 等待 */
			timeout_us = 400;
			us = 0;
			while (gpiod_get_value(dht11_gpio) && --timeout_us)
			{
				udelay(1);
				us++;
			}
			if (!timeout_us)
			{
				printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
				return -1;
			}
			
		}
		else
		{
			/* get bit 0 */
			data = (data << 1) | 0;
		}
	}

	*buf = data;
	return 0;
}
/* 4.2 向用户空间写入中数据，与用户的read函数对应 */
static ssize_t dht11_read(struct file *file, char __user *buf, size_t size, loff_t *offset){
    int i;
    int re;
	unsigned char data[5];
    unsigned long flags;
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    if(size != 4){
        return -EINVAL;
    }
    local_irq_save(flags);	  // 关中断
    gpiod_direction_output(dht11_gpio, 1);
	mdelay(30);
	gpiod_set_value(dht11_gpio, 0);
	mdelay(20);
	gpiod_set_value(dht11_gpio, 1);
	udelay(40);	
	gpiod_direction_input(dht11_gpio);	
	udelay(2);
    if (dht11_wait_for_ready())
	{
		local_irq_restore(flags); // 出错了，恢复中断
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		return -EAGAIN;
	}
	/* 3. 读5字节数据 */
	for (i = 0; i < 5; i++)
	{
		if (dht11_read_byte(&data[i]))
		{
			local_irq_restore(flags); // 恢复中断，出错了
			printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
			return -EAGAIN;
		}
	}
    gpiod_direction_output(dht11_gpio, 1);
    local_irq_restore(flags); // 恢复中断
    /* 4. 根据校验码验证数据 */
	if (data[4] != (data[0] + data[1] + data[2] + data[3]))
	{
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		//return -1;
	}
	printk("data %d %d\n", data[0], data[2]);
	re = copy_to_user(buf, data, 4);
	return 4;
}
static int dht11_release (struct inode *node, struct file *filp){
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    // free_irq(irq, NULL);
    // gpio_direction_output(dht11_num, 1);
    return 0;
}
/* 4. 定义file_operations结构体 */
static struct file_operations dht11_fop = {
    .owner      = THIS_MODULE,
    .open       = dht11_open,      // 没有open函数也可以打开设备
    .read      = dht11_read,
    .release = dht11_release,
};

/* 5.1 实现platform_driver的probe函数 */
// 在这个函数能够获取device的resource，并进行device_create
static int dht11_probe(struct platform_device *dev)  // 通过参数struct platform_device *dev获得device中的resource，去操作硬件
{
    /* 5.1.1 获取硬件信息 */
    // 在设备树中定义有：dht11-gpios = <...>
    dht11_gpio = gpiod_get(&dev->dev, "dht11", GPIOD_OUT_HIGH);  // 先配置为输出引脚
    // dht11_num = desc_to_gpio(dht11_gpio);
    // // setup_timer(&dht11_timer, dht11_timer_expire, 0);
    // setup_timer(&dht11_timer, dht11_timer_expire, 0);
    // // 在获得引脚的时候就已经设置了方向，下面就不需要设置方向了
    // /* 5.1.3 获取中断号 */    
    // irq = gpiod_to_irq(dht11_gpio);
    /* 5.1.4 开中断 */
    // 为irq中断注册了一个dht11_isr中断服务函数，上升沿触发
    // err = request_irq(irq, dht11_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "imx6ull_dht11", NULL);
    device_create(dht11_class, NULL, MKDEV(major, 0), NULL, "querydht11");
    return 0; 
}
/* 5.2 实现platform_driver的remove函数 */
static int dht11_remove(struct platform_device *dev)
{
    /* 5.2.1 销毁设备 */
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    device_destroy(dht11_class, MKDEV(major, 0)); /* /dev/dht110,1,... */
    // free_irq(irq, NULL);
    // del_timer(&dht11_timer);
    /* C. 5.2.4 销毁gpios */
	gpiod_put(dht11_gpio);
    return 0;
}
/* A. 5.3 实现platform_driver的of_device_id，用于支持设备树 */
static const struct of_device_id dht11_match[] = {
	{ .compatible = "imx6ull, dht11" },
	{},
};

/* 5. 定义platform_driver结构体 */
static struct platform_driver dht11_driver = {
    .probe = dht11_probe,      // 当配对时调用probe函数
    .remove = dht11_remove,    // 当取消设备时调用remove函数
    .driver = {
        .name = "dht11",      // 名字和platform_device配对（设备树下最后才匹配这个，一般来说不用它）
        .of_match_table = dht11_match,  // 定义of_match_table，支持设备树
    },
};

/* 1. 定义入口函数 */
static int __init dht11_init(void){   // __init为段属性，表示入口函数只使用一次，用过之后可以释放
    printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

    /* 1.1 注册字符设备驱动 */
    major = register_chrdev(0, "dht11", &dht11_fop);  // 参数为"主设备号（设置为0表示自动分配）、驱动名称、file_operations结构体"，返回值为主设备号，注意major为全局变量，在卸载设备驱动时要用；这个函数仅仅是注册了设备节点，如果想使用，需要通过创建设备节点（主设备号和注册的设备节点相同）
    /* 1.2 创建一个类，提供设备信息 */
    dht11_class = class_create(THIS_MODULE, "dht11_class");  // 参数为THIS_MODULE、类的名称，返回值为一个类（全局变量）；在/sys/class/文件下创建了一个hello_class的子目录
    /* 1.3 注册platform_driver */
    platform_driver_register(&dht11_driver);
    return 0;
}

/* 2. 定义出口函数 */
static void __exit dht11_exit(void){   // __exit在驱动编进内核时才起作用，这时就不需要出口函数了   
    /* 2.1 销毁platform_driver */
    platform_driver_unregister(&dht11_driver);
    /* 2.2 销毁类 */
    class_destroy(dht11_class);
    /* 2.3 卸载设备驱动 */
    unregister_chrdev(major, "dht11");   // 参数为"主设备号、驱动名称"
}

/* 3. 注册入口出口函数 */
module_init(dht11_init);     // 注册入口函数，也可以用init_module的方法注册
module_exit(dht11_exit);

MODULE_LICENSE("GPL");