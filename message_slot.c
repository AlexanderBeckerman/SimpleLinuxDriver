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
// #include <errno.h>
#include "message_slot.h"
MODULE_LICENSE("GPL");

typedef struct channel
{ // Each message slot will have different channels
  int minor;
  char msg[BUF_LEN];
  int msg_len;
  int id;
  struct channel *next;

} channel;

typedef struct slot
{
  int minor;
  struct channel *channel_head;
  struct slot *next;
} slot;

static slot *slots_head;

void displayLinkedList(void) {
    slot* currentSlot = slots_head;
    int counter = 0;
    while (currentSlot != NULL && counter < 7) {
        printk("------Slot %d-----", currentSlot->minor);

        channel* currentChannel = currentSlot->channel_head;
        while (currentChannel != NULL) {
            printk("Channel %d -> with msg_len = %d", currentChannel->id, currentChannel->msg_len);
            currentChannel = currentChannel->next;
        }

        currentSlot = currentSlot->next;
        counter++;
    }
}

// Function to create a new inner node
static channel *create_channel(int id)
{
  channel *newNode = (channel *)kmalloc(sizeof(channel), GFP_KERNEL);
  newNode->id = id;
  newNode->msg_len = 0;
  newNode->minor = -1;
  newNode->next = NULL;
  return newNode;
}

// Function to create a new outer node
static slot *create_slot(int minor)
{
  slot *newNode = (slot *)kmalloc(sizeof(slot), GFP_KERNEL);
  newNode->channel_head = NULL;
  newNode->minor = minor;
  newNode->next = NULL;
  return newNode;
}

// Function to insert a new Channel node at the end of the Channel linked list
static channel *insert_channel(channel **head, int id)
{
  channel *newChannelNode = create_channel(id);
  if (*head == NULL)
  {
    // If the list is empty, make the new node as the head
    *head = newChannelNode;
    return newChannelNode;
  }
  else
  {
    // Traverse the list to find the last node and append the new node
    channel *curr = *head;
    while (curr->next != NULL)
    {
      curr = curr->next;
    }
    curr->next = newChannelNode;
    return newChannelNode;
  }
}

// Function to insert a new outer node at the beginning of the outer linked list
static slot *insert_slot(int minor)
{
  slot *newSlotNode = create_slot(minor);

  if (slots_head == NULL)
  {
    // If the list is empty, make the new node as the head
    slots_head = newSlotNode;
    return newSlotNode;
  }
  else
  {
    // Traverse the list to find the last node and append the new node
    slot *curr = slots_head;
    while (curr->next != NULL)
    {
      curr = curr->next;
    }
    curr->next = newSlotNode;
    return newSlotNode;
  }
}


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
  int bytes_read = 0;
  int i;
  channel * chnl = (channel *)file->private_data;
  if (chnl == NULL)
  { // Check if such channel exists
    // errno = EINVAL;
    return -1;
  }
  if (chnl->msg_len == 0)
  { // Check if channel message not empty
    // errno = EWOULDBLOCK;
    return -1;
  }
  if (length < chnl->msg_len)
  { // Check if provided length is less than the channel message length
    // errno = ENOSPC;
    return -1;
  }
  for (i = 0; i < length && i < chnl->msg_len; i++)
  {
    if (put_user(chnl->msg[i], &buffer[i]) != 0)
    {
      // errno = EBADMSG;
      return -1;
    }
    bytes_read++;
  }

  return bytes_read;
}
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset)
{
  char the_message[BUF_LEN]; // We use a middle buffer to copy to so we can make sure the write was atomic
  int i;
  channel *chnl = (channel *)file->private_data;
  if (file->private_data == NULL)
  { // Check if channel id exists
    // errno = EINVAL;
    printk("private data was NULL!");
    return -1;
  }
  if (chnl == NULL)
  { // Check if such channel exists
    // errno = EINVAL;
    printk("chnl was NULL!");
    return -1;
  }

  if (length <= 0 || length > BUF_LEN)
  {
    // errno = EMSGSIZE;
    printk("invalid length");
    return -1;
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  { // Copy the users message to our middle buffer
    if (get_user(the_message[i], &buffer[i]) != 0)
    {
      // errno = EBADMSG;
      printk("failed inside get_user loop");
      return -1;
    }
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  { // Copy the users message to our channel buffer
    chnl->msg[i] = the_message[i];
  }
  chnl->msg_len = length;
  printk("wrote the message %s of length %d", chnl->msg, chnl->msg_len);
  displayLinkedList();
  return chnl->msg_len;
}
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param)
{
  if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)
  {
    // errno = EINVAL;
    return -1;
  }
  // printk("inside ioctl line 138 with param = %d", ioctl_param);
  int minor = iminor(file->f_inode);
  slot *curr = slots_head;
  while (curr->next != NULL)
  {
    if (curr->minor == minor)
    {
      break;
    }
    curr = curr->next;
  }
  channel *chnl_head = curr->channel_head;
  if (curr->channel_head == NULL)
  {
    channel *newChannel = insert_channel(&(curr->channel_head), ioctl_param);
    newChannel->minor = minor;
    newChannel->id = ioctl_param;
    newChannel->msg_len = 0;
    file->private_data = (void *)newChannel;
    // printk("head was NULL. Created a new channel with id %d", newChannel->id);
    // displayLinkedList();
    return SUCCESS;
  }
  while (chnl_head->next != NULL)
  {
    if (chnl_head->id == ioctl_param)
    {
      // printk("found existing channel with id %d", chnl_head->id);
      // displayLinkedList();
      file->private_data = (void *)chnl_head;
      return SUCCESS;
    }
    chnl_head = chnl_head->next;
  }
  if (chnl_head->id == ioctl_param)
  {
    // printk("found existing channel with id %d", chnl_head->id);
    // displayLinkedList();
    file->private_data = (void *)chnl_head;
    return SUCCESS;
  }
  channel *newChannel = insert_channel(&(curr->channel_head), ioctl_param);
  newChannel->minor = minor;
  newChannel->id = ioctl_param;
  newChannel->msg_len = 0;
  file->private_data = (void *)newChannel;
  // printk("Created a new channel with id %d", newChannel->id);
  // displayLinkedList();
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
  slots_head = NULL;
  for (i = 0; i < MAX_MINOR_NUM; i++)
  {
    insert_slot(i);
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

module_init(simple_init);
module_exit(simple_cleanup);
