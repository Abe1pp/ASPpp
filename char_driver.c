#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/moduleparam.h>

#define MYDEV_NAME "mycdrv"

#define ramdisk_size (size_t)(16 * PAGE_SIZE) // ramdisk size

// NUM_DEVICES defaults to 3 unless specified during insmod
int NUM_DEVICES = 3;

#define CDRV_IOC_MAGIC 'Z'
#define ASP_CLEAR_BUF _IO(CDRV_IOC_MAGIC, 1)

#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

typedef struct ASP_mycdrv
{
	struct cdev dev;
	char *ramdisk;
	struct semaphore sem;
	int devNo;
	// any other field you may want to add
	size_t ram_size;
	size_t end_of_buf;
	int count;
} mycdrv_obj;

int major = 500;
dev_t dev_first;
struct class *dev_cls;
mycdrv_obj *device;


ssize_t cdrv_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos)
{
	mycdrv_obj *mycdrv;
	int nbytes = 0;
	mycdrv = (mycdrv_obj *)file->private_data;

	if (down_interruptible(&mycdrv->sem))
		return -ERESTARTSYS;

	if ((lbuf + *ppos) > mycdrv->ram_size)
	{
		up(&mycdrv->sem);
		return 0;
	}
	nbytes = lbuf - copy_to_user(buf, mycdrv->ramdisk + *ppos, lbuf);
	*ppos += nbytes;
	up(&mycdrv->sem);
	return nbytes;
}

ssize_t cdrv_write(struct file *file, const char __user *buf, size_t lbuf, loff_t *ppos)
{
	mycdrv_obj *mycdrv;
	int nbytes = 0;

	mycdrv = (mycdrv_obj *)file->private_data;
	if (down_interruptible(&mycdrv->sem))
		return -ERESTARTSYS;

	if ((lbuf + *ppos) > mycdrv->ram_size)
		return 0;

	nbytes = lbuf - copy_from_user(mycdrv->ramdisk + *ppos, buf, lbuf);
	*ppos += nbytes;
	if (mycdrv->end_of_buf < *ppos)
		mycdrv->end_of_buf = *ppos;
	up(&mycdrv->sem);
	return nbytes;
}


int cdrv_open(struct inode *inode, struct file *file)
{
	mycdrv_obj *mycdrv;
	mycdrv = container_of(inode->i_cdev, struct ASP_mycdrv, dev);
	file->private_data = mycdrv;

	if (down_interruptible(&mycdrv->sem))
		return -ERESTARTSYS;
	mycdrv->count++;
	up(&mycdrv->sem);

	return 0;
}


loff_t cdrv_lseek(struct file *file, loff_t offset, int orig)
{
	mycdrv_obj *mycdrv;
	size_t size_original;
	size_t size_new;
	size_t idx;
	char *ramdisk_new;

	mycdrv = (mycdrv_obj *)file->private_data;
	
	switch (orig)
	{
	case SEEK_SET:
		file->f_pos = offset;
		
		break;

	case SEEK_CUR:
		file->f_pos += offset;
		break;

	case SEEK_END:
		file->f_pos = mycdrv->end_of_buf + offset;

		size_original = mycdrv->ram_size;
		size_new = mycdrv->ram_size + (offset * sizeof(char));

		if ((ramdisk_new = (char *)kzalloc(size_new, GFP_KERNEL)) != NULL)
		{
			for (idx = 0; idx < size_original; idx++)
				ramdisk_new[idx] = mycdrv->ramdisk[idx];
			kfree(mycdrv->ramdisk);
			mycdrv->ramdisk = ramdisk_new;
			mycdrv->ram_size = size_new;
		}
		break;
	default:
		break;
	}

	file->f_pos = file->f_pos >= 0 ? file->f_pos : 0;
	up(&mycdrv->sem);
	return file->f_pos;
}

long cdrv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	mycdrv_obj *mycdrv;
	mycdrv = (mycdrv_obj *)file->private_data;

	switch (cmd)
	{
	case ASP_CLEAR_BUF:
		if (down_interruptible(&mycdrv->sem))
			return -ERESTARTSYS;
		memset((void *)mycdrv->ramdisk, 0, mycdrv->ram_size);
		mycdrv->end_of_buf = 0;
		file->f_pos = 0;
		up(&mycdrv->sem);
		break;

	default:
		return -1;
	}
	return 0;
}

const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.read = cdrv_read,
	.write = cdrv_write,
	.open = cdrv_open,
	.llseek = cdrv_lseek,
	.unlocked_ioctl = cdrv_ioctl,
};

int my_init(void)
{
	unsigned int minor = 0;
	int nor;
	if (alloc_chrdev_region(&dev_first, minor, NUM_DEVICES, MYDEV_NAME) != 0)
		return -1;
	major = MAJOR(dev_first);
	if ((dev_cls = class_create(THIS_MODULE, MYDEV_NAME)) == NULL)
		return -1;
	device = (mycdrv_obj *)kzalloc(NUM_DEVICES * sizeof(mycdrv_obj), GFP_KERNEL);

	for (nor = 0; nor < NUM_DEVICES; nor++)
	{
		dev_t dev_num = MKDEV(major, nor);
		mycdrv_obj *dptr = &device[nor];
		dptr->dev.owner = THIS_MODULE;
		cdev_init(&dptr->dev, &mycdrv_fops);
		dptr->ram_size = ramdisk_size;
		dptr->devNo = nor;
		dptr->count = 0;
		dptr->end_of_buf = 0;
		dptr->ramdisk = (char *)kzalloc(ramdisk_size, GFP_KERNEL);
		cdev_add(&dptr->dev, dev_num, 1);
		sema_init(&dptr->sem, 1);
		device_create(dev_cls, NULL, dev_num, NULL, MYDEV_NAME "%d", nor);
	}
	return 0;
}

void my_exit(void)
{
	int nor;
	for (nor = 0; nor < NUM_DEVICES; nor++)
	{
		mycdrv_obj *dptr = &device[nor];
		cdev_del(&dptr->dev);
		kfree(dptr->ramdisk);
		device_destroy(dev_cls, MKDEV(major, nor));
	}
	kfree(device);
	class_destroy(dev_cls);
	unregister_chrdev_region(dev_first, NUM_DEVICES);
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("user");
MODULE_LICENSE("GPL v2");