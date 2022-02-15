#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <linux/uaccess.h>


#include <linux/io.h> //iowrite ioread
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>//ioremap



 MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for matrix ouvput");
#define DEVICE_NAME "matrix"
#define DRIVER_NAME "matrix_driver"
#define BUFF_SIZE 20


//*******************FUNCTION PROTOTYPES************************************

static int matrix_probe(struct platform_device *pdev);
static int matrix_open(struct inode *i, struct file *f);
static int matrix_close(struct inode *i, struct file *f);
static ssize_t matrix_read(struct file *f, char __user *buf, size_t len, loff_t *off);
static ssize_t matrix_write(struct file *f, const char __user *buf, size_t length, loff_t *off);
static int __init matrix_init(void);
static void __exit matrix_exit(void);
static int matrix_remove(struct platform_device *pdev);

//*********************GLOBAL VARIABLES*************************************
struct matrix_info {
  unsigned long mem_start;
  unsigned long mem_end;
  void __iomem *base_addr;
};
static struct cdev *my_cdev;
static dev_t my_dev_id;
static struct class *my_class;
static struct device *my_device;
static int int_cnt;
static struct matrix_info *vp = NULL;

static struct file_operations my_fops =
  {
    .owner = THIS_MODULE,
    .open = matrix_open,
    .release = matrix_close,
    .read = matrix_read,
    .write = matrix_write
  };
static struct of_device_id matrix_of_match[] = {
  { .compatible = "xlnx,axi-bram-ctrl-A", }, //40000000
  { .compatible = "xlnx,axi-bram-ctrl-B", }, //42000000
  { .compatible = "xlnx,axi-bram-ctrl-C", }, //44000000
  { .compatible = "xlnx,matrix-multiplier", }, //43c00000
  { /* end of list */ },
};

static struct platform_driver matrix_driver = {
  .driver = {
    .name = DRIVER_NAME,
    .owner = THIS_MODULE,
    .of_match_table	= matrix_of_match,
  },
  .probe		= matrix_probe,
  .remove	= matrix_remove,
};

MODULE_DEVICE_TABLE(of, matrix_of_match);


//***************************************************************************

// PROBE AND REMOVE

static int matrix_probe(struct platform_device *pdev)
{
  struct resource *r_mem;
  int rc = 0;

  printk(KERN_INFO "Probing\n");
  // Get phisical register adress space from device tree
  r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!r_mem) {
    printk(KERN_ALERT "matrix_probe: Failed to get reg resource\n");
    return -ENODEV;
  }
  // Get memory for structure matrix_info
  vp = (struct matrix_info *) kmalloc(sizeof(struct matrix_info), GFP_KERNEL);
  if (!vp) {
    printk(KERN_ALERT "matrix_probe: Could not allocate timer device\n");
    return -ENOMEM;
  }
  // Put phisical adresses in timer_info structure
  vp->mem_start = r_mem->start;
  vp->mem_end = r_mem->end;
    
  // Reserve that memory space for this driver
  if (!request_mem_region(vp->mem_start,vp->mem_end - vp->mem_start + 1, DRIVER_NAME))
  {
    printk(KERN_ALERT "matrix_probe: Could not lock memory region at %p\n",(void *)vp->mem_start);
    rc = -EBUSY;
    goto error1;
  }    
  // Remap phisical to virtual adresses

  vp->base_addr = ioremap(vp->mem_start, vp->mem_end - vp->mem_start + 1);
  if (!vp->base_addr) {
    printk(KERN_ALERT "matrix_probe: Could not allocate memory\n");
    rc = -EIO;
    goto error2;
  }

  printk(KERN_NOTICE "matrix_probe: matrix platform driver registered\n");
  return 0;//ALL OK
 error2:
  release_mem_region(vp->mem_start, vp->mem_end - vp->mem_start + 1);
 error1:
  return rc;

}

static int matrix_remove(struct platform_device *pdev)
{
  int i = 0;
  // Exit Device Module
  for (i = 0; i < (256*144); i++) 
  { 
    iowrite32(i*4, vp->base_addr + 8); 
    iowrite32(0, vp->base_addr); 
  } 
  printk(KERN_INFO "matrix_remove: matrix remove in process");
  iounmap(vp->base_addr);
  release_mem_region(vp->mem_start, vp->mem_end - vp->mem_start + 1);
  kfree(vp);
  printk(KERN_INFO "matrix_remove: matrix driver removed");
  return 0;
}

//***************************************************
// IMPLEMENTATION OF FILE OPERATION FUNCTIONS

static int matrix_open(struct inode *i, struct file *f)
{
  //printk(KERN_INFO "matrix opened\n");
  return 0;
}
static int matrix_close(struct inode *i, struct file *f)
{
  //printk(KERN_INFO "matrix closed\n");
  return 0;
}
static ssize_t matrix_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
  //printk("matrix read\n");
  return 0;
}
static ssize_t matrix_write(struct file *f, const char __user *buf, size_t length, loff_t *off)
{	
  char buff[BUFF_SIZE];
  int ret = 0;
  unsigned int xpos=0,ypos=0;
  unsigned long long rgb=0;
  unsigned char rgb_buff[10];  
  ret = copy_from_user(buff, buf, length);  
  if(ret){
    printk("copy from user failed \n");
    return -EFAULT;
  }  
  buff[length] = '\0';
  
  
  sscanf(buff,"%d,%d,%s", &xpos, &ypos, rgb_buff);  
  ret = kstrtoull(rgb_buff, 0, &rgb);
 
  if(ret != -EINVAL)//checking for parsing error
  {
    if (xpos > 255)
    {
      printk(KERN_WARNING "matrix_write: X_axis position exceeded, maximum is 255 and minimum 0 \n");
    }
    else if (ypos > 143)
    {
      printk(KERN_WARNING "matrix_write: Y_axis position exceeded, maximum is 143 and minimum 0 \n");
    }
    else
    {
      iowrite32((256*ypos + xpos)*4, vp->base_addr + 8);
      iowrite32(rgb, vp->base_addr);      
    }
  }
  else
  {
    printk(KERN_WARNING "matrix_write: Wrong write format, expected \"xpos,ypos,rgb\"\n");
    // return -EINVAL;//parsing error
  }        
  return length;

}

//***************************************************
// HELPER FUNCTIONS (STRING TO INTEGER)


//***************************************************
// INIT AND EXIT FUNCTIONS OF THE DRIVER

static int __init matrix_init(void)
{

  int ret = 0;
  int_cnt = 0;

  printk(KERN_INFO "matrix_init: Initialize Module \"%s\"\n", DEVICE_NAME);
  ret = alloc_chrdev_region(&my_dev_id, 0, 1, "matrix_region");
  if (ret)
  {
    printk(KERN_ALERT "<1>Failed CHRDEV!.\n");
    return -1;
  }
  printk(KERN_INFO "Succ CHRDEV!.\n");
  my_class = class_create(THIS_MODULE, "matrix_drv");
  if (my_class == NULL)
  {
    printk(KERN_ALERT "<1>Failed class create!.\n");
    goto fail_0;
  }
  printk(KERN_INFO "Succ class chardev1 create!.\n");
  my_device = device_create(my_class, NULL, MKDEV(MAJOR(my_dev_id),0), NULL, "matrix");
  if (my_device == NULL)
  {
    goto fail_1;
  }

  printk(KERN_INFO "Device created.\n");

  my_cdev = cdev_alloc();	
  my_cdev->ops = &my_fops;
  my_cdev->owner = THIS_MODULE;
  ret = cdev_add(my_cdev, my_dev_id, 1);
  if (ret)
  {
    printk(KERN_ERR "matrix_init: Failed to add cdev\n");
    goto fail_2;
  }
  printk(KERN_INFO "matrix Device init.\n");

  return platform_driver_register(&matrix_driver);

 fail_2:
  device_destroy(my_class, MKDEV(MAJOR(my_dev_id),0));
 fail_1:
  class_destroy(my_class);
 fail_0:
  unregister_chrdev_region(my_dev_id, 1);
  return -1;

} 

static void __exit matrix_exit(void)  		
{

  platform_driver_unregister(&matrix_driver);
  cdev_del(my_cdev);
  device_destroy(my_class, MKDEV(MAJOR(my_dev_id),0));
  class_destroy(my_class);
  unregister_chrdev_region(my_dev_id, 1);
  printk(KERN_INFO "matrix_exit: Exit Device Module \"%s\".\n", DEVICE_NAME);
}

module_init(matrix_init);
module_exit(matrix_exit);

MODULE_AUTHOR ("FTN");
MODULE_DESCRIPTION("Test Driver for matrix output.");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("custom:matrix");
