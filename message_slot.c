#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>  /* We're doing kernel work */
#include <linux/module.h>  /* Specifically, a module */
#include <linux/fs.h>      /* for register_chrdev */
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/string.h>  /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>    /* for kmalloc GFP flag */
#include <errno.h>

MODULE_LICENSE("GPL");
#include <message_slot.h>

typedef struct channel
{ // Each message slot will have different channels
  int minor;
  char msg[BUF_LEN];
  int msg_len;
  int id;
  channel *next;

} channel;

channel *channel_lists[MAX_MINOR_NUM]; /* This will hold a pointer to a list of the channels of each device.
                           Minor numbers do not exceed 256, so we can use a fixed sized array for all possible minor numbers. */

static int device_open(struct inode *inode,
                       struct file *file)
{

  return SUCCESS;
}

static int device_release(struct inode *inode,
                          struct file *file)
{
  return SUCCESS;
}
static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset)
{

  return SUCCESS;
}
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset)
{
  char the_message[BUF_LEN]; // We use a temporary buffer to copy to so we can make sure the write was atomic
  int minor = iminor(file->f_inode);
  if(file->private_data == NULL){
    errno = EINVAL;
    return -1;
  }
  channel * f_chnl = find_channel((int)file->private_data, minor);
  if (f_chnl == NULL)
  {
    errno = EINVAL;
    return -1;
  }

  if (length <= 0 || length > BUF_LEN)
  {
    errno = EMSGSIZE;
    return -1;
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  {
    if (get_user(the_message[i], &buffer[i]) != 0)
    {
      errno = EBADMSG;
      return -1;
    }
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  {
    f_chnl->msg[i] = the_message[i];
  }
  f_chnl->msg_len = length;

  return SUCCESS;
}
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param)
{
  int minor = iminor(file->f_inode);
  channel *head = channel_lists[minor]; // Get the current channels corresponding to the inode
  if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)
  {
    errno = EINVAL;
    return -1;
  }

  file->private_data = (void *)ioctl_param; // Store the id in the file private data
  if (head == NULL)
  { // If we don't have any channels yet, we want to create a new channel
    head = kmalloc(sizeof(channel), GFP_KERNEL);
    if (head == NULL)
    { // Check for failed kmalloc
      errno = ENOMEM;
      return -1;
    }
    head->next = NULL;
    head->minor = minor;
    head->id = ioctl_param;
    head->msg = NULL;
    head->msg_len = 0;
    return SUCCESS;
  }
  else
  {
    channel *temp = head;
    while (temp->next != NULL)
    { // Iterate over the channels and create the channel if needed
      if (temp->id == ioctl_param)
      { // Channel already exists
        return SUCCESS;
      }
    }
    if (temp->id == ioctl_param)
    { // We stopped one channel before the end so we check the last channel id
      return SUCCESS;
    }
    temp->next = kmalloc(sizeof(channel), GFP_KERNEL);
    if (temp->next == NULL)
    { // Check for failed kmalloc
      errno = ENOMEM;
      return -1;
    }
    temp = temp->next;
    temp->next = NULL;
    temp->minor = minor;
    temp->id = ioctl_param;
    temp->msg = NULL;
    temp->msg_len = 0;
  }

  return SUCCESS;
}

struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

static int __init simple_init(void)
{
  int rc = -1;
  int i = 0;

  for (i; i < MAX_MINOR_NUM; i++) // Initialize empty channel list array
  {
    channel_lists[i] = NULL;
  }

  // Register driver capabilities. Obtain major num
  rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);

  // Negative values signify an error
  if (rc < 0)
  {
    printk(KERN_ERR "%s registraion failed\n");
    return rc;
  }

  return SUCCESS;
}

static void __exit simple_cleanup(void)
{
  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

static channel * find_channel(int qid, int minor)
{
  channel * temp = channel_lists[minor];
  if(temp == NULL){
    return NULL;
  }
  while(temp != NULL){
    if(temp->id == qid){
      return temp;
    }
  }
  return NULL;
}

module_init(simple_init);
module_exit(simple_cleanup);
