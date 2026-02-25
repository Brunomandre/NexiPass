
#include <linux/init.h>
#include <linux/fs.h>       
#include <linux/cdev.h>     
#include <linux/io.h>   
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/device.h>  
#include <linux/kernel.h>   
#include <linux/err.h>    

#include "utility.h"

#define DEVICE_NAME "ledrgb"       
#define CLASS_NAME  "ledrgbclass"
#define NUM_DEVICES 1


MODULE_LICENSE("GPL");

static struct class *led_class;
static dev_t DEV_major_minor;
static struct cdev c_dev;

static GPIORegister* s_GPIO;

// GPIOs: Red=23, Green=24, Blue=25
static const int PIN_RED   = 23;
static const int PIN_GREEN = 24;
static const int PIN_BLUE  = 25;

int led_device_open (struct inode* p_indode, struct file *p_file){
    pr_alert("%s: called\n",__FUNCTION__);
	p_file->private_data = (GPIORegister *) s_GPIO;
	return 0;
}

int led_device_close (struct inode* p_indode, struct file *p_file){
    pr_alert("%s: called\n",__FUNCTION__);
	p_file->private_data = NULL;
	return 0;
}

ssize_t led_device_read(struct file *pfile, char __user *p_buff,size_t len, loff_t *poffset){
	pr_alert("%s: called (%zu)\n",__FUNCTION__,len);
    return 0;
}

ssize_t led_device_write(struct file *pfile, const char __user *buf, size_t len, loff_t *offset){
    
    pr_alert("%s: called (%zu)\n",__FUNCTION__,len);

    if (unlikely(pfile->private_data == NULL)) 
        return -EFAULT;

    if (len < 3) {
        pr_warn("ledrgb: insufficient data, need 3 bytes (RGB)\n");
        return -EINVAL;
    }

    char kbuf[3];
    if (copy_from_user(kbuf, buf, 3))
        return -EFAULT;

    if ((kbuf[0] != '0' && kbuf[0] != '1') ||
        (kbuf[1] != '0' && kbuf[1] != '1') ||
        (kbuf[2] != '0' && kbuf[2] != '1')) {
        pr_warn("ledrgb: invalid input, expected '0' or '1' for each RGB component\n");
        return len;
    }

    GPIORegister* pdev = (GPIORegister*) pfile->private_data;
	
    // Red
    if (kbuf[0] == '0')
        SetGPIOState(pdev, PIN_RED, 0);
    else
        SetGPIOState(pdev, PIN_RED, 1);

    // Green
    if (kbuf[1] == '0')
        SetGPIOState(pdev, PIN_GREEN, 0);
    else
        SetGPIOState(pdev, PIN_GREEN, 1);

    // Blue
    if (kbuf[2] == '0')
        SetGPIOState(pdev, PIN_BLUE, 0);
    else
        SetGPIOState(pdev, PIN_BLUE, 1);
    
    pr_info("ledrgb: Set RGB to %c%c%c\n", kbuf[0], kbuf[1], kbuf[2]);
    
    return len;
}

//comunicação entre o DD e o kernel
static struct file_operations ledDevice_fops  = {
        .owner = THIS_MODULE,
	.write = led_device_write,
	.read = led_device_read,
	.release = led_device_close,
	.open = led_device_open,
};


static int __init 
    led_init (void){
    
    int ret;
    struct device *dev_ret;

    pr_alert ("RGB LED Driver Init function called\n");

    if ((ret = alloc_chrdev_region(&DEV_major_minor, 0, NUM_DEVICES, DEVICE_NAME)) != 0){
       printk(KERN_DEBUG "Can't register device\n"); return ret;
    }

    if (IS_ERR(led_class = class_create(CLASS_NAME))){
        unregister_chrdev_region (DEV_major_minor, NUM_DEVICES);
        return (PTR_ERR(led_class));
    }


    if (IS_ERR(dev_ret = device_create(led_class, NULL, DEV_major_minor, NULL, DEVICE_NAME))) {
        class_destroy(led_class);
        unregister_chrdev_region (DEV_major_minor, NUM_DEVICES);
        return PTR_ERR(dev_ret);
    }


    cdev_init(&c_dev, &ledDevice_fops);
    c_dev.owner = THIS_MODULE;


	if ((ret = cdev_add(&c_dev, DEV_major_minor, NUM_DEVICES)) < 0) {
		printk(KERN_NOTICE "Error %d adding device", ret);
		device_destroy(led_class, DEV_major_minor);
		class_destroy(led_class);
		unregister_chrdev_region(DEV_major_minor, NUM_DEVICES);
		return ret;
	}

    s_GPIO = (GPIORegister *)ioremap(GPIO_BASE_ADDR, sizeof(GPIORegister));
	pr_alert("Map to virtual address: %p\n", s_GPIO);

    // Configure RGB pins as outputs
    SetGPIOFunction(s_GPIO, PIN_RED, OUTPUT);
    SetGPIOFunction(s_GPIO, PIN_GREEN, OUTPUT);
    SetGPIOFunction(s_GPIO, PIN_BLUE, OUTPUT);

    SetGPIOState(s_GPIO, PIN_RED, 0);
    SetGPIOState(s_GPIO, PIN_GREEN, 0);
    SetGPIOState(s_GPIO, PIN_BLUE, 0);

    pr_info("RGB LED Driver initialized: R=GPIO%d, G=GPIO%d, B=GPIO%d\n", 
            PIN_RED, PIN_GREEN, PIN_BLUE);

    return 0;
}

static void __exit 
    cleanup (void){

    pr_alert ("RGB LED Driver Exit function called\n");

    // Coloca os pinos no estado DEFAULT
    SetGPIOState(s_GPIO, PIN_RED, 0);
    SetGPIOState(s_GPIO, PIN_GREEN, 0);
    SetGPIOState(s_GPIO, PIN_BLUE, 0);
    
    SetGPIOFunction(s_GPIO, PIN_RED, INPUT);
    SetGPIOFunction(s_GPIO, PIN_GREEN, INPUT);
    SetGPIOFunction(s_GPIO, PIN_BLUE, INPUT);

    iounmap (s_GPIO);
    cdev_del(&c_dev);
    device_destroy(led_class, DEV_major_minor);
    class_destroy(led_class);
    unregister_chrdev_region(DEV_major_minor, NUM_DEVICES);
}

module_init(led_init);
module_exit(cleanup);
