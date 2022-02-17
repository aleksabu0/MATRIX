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
void extract_matrix(char store_mat[50], int mat[50],int dim[]);
int myAtoi(char* str);
void myItoa(int num, char* str);
void reverse(char s[]); 

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
static struct matrix_info *vp[4] ;//= NULL; //array of struct

int dimA[] = {0, 0};
int dimB[] = {0, 0};
int matA[50], matB[50];
char store_matA[50], store_matB[50];
int cnt=0;
int endRead =0;

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
  // for vp array

  printk(KERN_INFO "Probing\n");
  // Get phisical register adress space from device tree
  r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!r_mem) {
    printk(KERN_ALERT "matrix_probe: Failed to get reg resource\n");
    return -ENODEV;
  }
  // Get memory for structure matrix_info
  vp[cnt] = (struct matrix_info *) kmalloc(sizeof(struct matrix_info), GFP_KERNEL);
  if (!vp[cnt]) {
    printk(KERN_ALERT "matrix_probe: Could not allocate memory\n");
    return -ENOMEM;
  }
  // Put phisical adresses in timer_info structure
  vp[cnt]->mem_start = r_mem->start;
  vp[cnt]->mem_end = r_mem->end;
  printk(KERN_INFO "matrix_probe: MEMORY LOCATION %px\n",(void *)r_mem->start);  
  // Reserve that memory space for this driver
  if (!request_mem_region(vp[cnt]->mem_start,vp[cnt]->mem_end - vp[cnt]->mem_start + 1, DRIVER_NAME))
  {
    printk(KERN_ALERT "matrix_probe: Could not lock memory region at %p\n",(void *)vp[cnt]->mem_start);
    rc = -EBUSY;
    goto error1;
  }    
  // Remap phisical to virtual adresses

  vp[cnt]->base_addr = ioremap(vp[cnt]->mem_start, vp[cnt]->mem_end - vp[cnt]->mem_start + 1);
  if (!vp[cnt]->base_addr) {
    printk(KERN_ALERT "matrix_probe: Could not allocate memory\n");
    rc = -EIO;
    goto error2;
  }

  printk(KERN_NOTICE "matrix_probe: matrix platform driver registered\n");
  cnt++;
  if(cnt==4)
  {
	  cnt=0;
  }  
  return 0;//ALL OK
 error2:
  release_mem_region(vp[cnt]->mem_start, vp[cnt]->mem_end - vp[cnt]->mem_start + 1);
 error1:
  return rc;

}

static int matrix_remove(struct platform_device *pdev)
{
  int i = 0;
  // Exit Device Module
  for (i = 0; i <= (32*49); i++) 
  { 
    iowrite32(i*4, vp[cnt]->base_addr + 8); 
    iowrite32(0, vp[cnt]->base_addr); 
  } 
  printk(KERN_INFO "matrix_remove: matrix remove in process");
  iounmap(vp[cnt]->base_addr);
  release_mem_region(vp[cnt]->mem_start, (vp[cnt]->mem_end - vp[cnt]->mem_start + 1));
  kfree(vp[cnt]);
  printk(KERN_INFO "matrix_remove: matrix driver removed");
  cnt++;
  return 0;
}

//***************************************************
// IMPLEMENTATION OF FILE OPERATION FUNCTIONS

static int matrix_open(struct inode *i, struct file *f)
{
  printk(KERN_INFO "matrix opened\n");
  return 0;
}
static int matrix_close(struct inode *i, struct file *f)
{
  printk(KERN_INFO "matrix closed\n");
  return 0;
}
static ssize_t matrix_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	int ret;
	int length = 0;
	u32 led_val = 0;
	int i = 0;
	char buff[BUFF_SIZE];
	if (endRead)
	{
		endRead = 0;
		return 0;
	}
	led_val = 1234;
	myItoa(led_val,buff);
	
	for (i = 0; buff[i] != '\0'; i++);
    length = i;
	
	ret = copy_to_user(buf, buff, length);
	if(ret)
	return -EFAULT;
	printk(KERN_INFO "Succesfully read\n");
	endRead = 1;
	return length;

}
static ssize_t matrix_write(struct file *f, const char __user *buf, size_t length, loff_t *off)
{	
  char buff[BUFF_SIZE];
  int ret = 0;
  ret = copy_from_user(buff, buf, length);  
  if(ret){
    printk("copy from user failed \n");
    return -EFAULT;
  }  
    buff[length] = '\0';
  
    sscanf(buff, "%[^*]*%s" , store_matA, store_matB);

    extract_matrix(store_matA, matA, dimA);
    extract_matrix(store_matB, matB, dimB);
	
    if(dimA[1] != dimB[0]){
        //printf("\nError! DiffDim\n");
        return -1;
    }

    //printf("\ndimA: %dx%d", dimA[0],dimA[1]);
    //printf("\ndimB: %dx%d", dimB[0],dimB[1]);
 
  //if(ret != -EINVAL)//checking for parsing error
  //{
   // iowrite32((256*ypos + xpos)*4, vp->base_addr + 8);
    //iowrite32(rgb, vp->base_addr);         
  //}
  /*else
  {
    printk(KERN_WARNING "matrix_write: Wrong write format, expected \"xpos,ypos,rgb\"\n");
    // return -EINVAL;//parsing error
  }  */      
  return length;
}

//***************************************************
// HELPER FUNCTIONS (READ MATRIX)

void extract_matrix(char store_mat[50], int mat[50],int dim[])
{
    int i, j=0, k=0;
    int n=0, m=0;
    char num[5];
    int numlen=0;
	int len;
	
    for (i = 0; store_mat[i] != '\0'; i++);
    len = i;

    for (i = 1; i<len-1; i++)
    {
        if(store_mat[i]==']')
        {
            num[numlen]='\0';
            mat[j]=myAtoi(num);
            j++;

            for(i=0; i<5; i++)
            {
                num[i]=0;
            }
            numlen=0;
            m++;
            if(!k)
            {
                dim[1] = m;
                k++;
            }
            if(m != dim[1])
            {
                //printf("\nError!\n");
                //return -1;
            }
            m=0;
        }

        if(store_mat[i]=='[')
        {
            n++;
        }

        if(store_mat[i]==',' && store_mat[i+1]!='[')
        {
            num[numlen]='\0';
            mat[j]=myAtoi(num);
            j++;
            for(i=0; i<5; i++)
            {
                num[i]=0;
            }
            numlen=0;
            m++;

        }

        if(store_mat[i] >= 48 && store_mat[i] <= 57)
        {
            num[numlen]=store_mat[i];
            numlen++;
        }
    }
    dim[0]=n;
    if(dim[0] > 7 || dim[1] > 7){
        //printf("\nMaxDim : 7x7");
        //return -1;
    }
    for(i=0; i<dim[0]*dim[1];i++){
         if(mat[i] > 4096){
            //printf("\nMaxNum : 4096");
            //return -1;
         }
    }
}

int myAtoi(char* str)
{
    int res = 0;
	int i;
    for (i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
    return res;
}

void myItoa(int n, char s[])
 {
	 int i, sign;

	 if ((sign = n) < 0)  /* record sign */
		 n = -n;          /* make n positive */
	 i = 0;
	 do {       /* generate digits in reverse order */
		 s[i++] = n % 10 + '0';   /* get next digit */
	 } while ((n /= 10) > 0);     /* delete it */
	 if (sign < 0)
		 s[i++] = '-';
	 s[i] = '\0';
	 reverse(s);
 }

void reverse(char s[])
 {
     int i, j;
     char c;
 
     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }


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
  my_device = device_create(my_class, NULL, MKDEV(MAJOR(my_dev_id),0), NULL, "matmul");
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


