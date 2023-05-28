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
#include "message_slot.h"
MODULE_LICENSE("GPL");

typedef struct channel
{                       // Each message slot will have different channels
  char msg[BUF_LEN];    // Holds the message
  int msg_len;          // The message length
  int id;               // The channel id
  struct channel *next; // Pointer to the next channel node in the same slot

} channel;

typedef struct slot
{                               // Defines a message slot
  int minor;                    // Holds the slot minor
  struct channel *channel_head; // Pointer to the slots channel list
  struct slot *next;            // Next slot node
} slot;

typedef struct data
{ // Information for each file private data. Will hold the channel of the file and the minor.
  int minor;
  struct channel *file_channel;
} data;

static slot *slots_head; // I will use a linked list of slots where each slot holds a linked list of channels

// Note - I got the following list handling functions from the internet (with some changes i made)
// Function to create a new channel node
static channel *create_channel(int id)
{
  channel *new_node = (channel *)kmalloc(sizeof(channel), GFP_KERNEL);
  if (new_node == NULL)
  {
    return NULL;
  }
  new_node->id = id;
  new_node->msg_len = 0;
  new_node->next = NULL;
  return new_node;
}

// Function to create a new slot node
static slot *create_slot(int minor)
{
  slot *new_slot = (slot *)kmalloc(sizeof(slot), GFP_KERNEL);
  if (new_slot == NULL)
  {
    return NULL;
  }
  new_slot->channel_head = NULL;
  new_slot->minor = minor;
  new_slot->next = NULL;
  return new_slot;
}

// Function to insert a new channel node at the end of the channel linked list
static channel *insert_channel(channel **head, int id)
{
  channel *new_channel = create_channel(id);
  if (*head == NULL)
  {
    // If the list is empty, make the new node as the head
    *head = new_channel;
    return new_channel;
  }
  else
  {
    // Traverse the list to find the last node and append the new node
    channel *curr = *head;
    while (curr->next != NULL)
    {
      curr = curr->next;
    }
    curr->next = new_channel;
    return new_channel;
  }
}

// Function to insert a new slot node at the end of the slot linked list
static slot *insert_slot(int minor)
{
  slot *new_slot = create_slot(minor);

  if (slots_head == NULL)
  {
    // If the list is empty, make the new node as the head
    slots_head = new_slot;
    return new_slot;
  }
  else
  {
    // Traverse the list to find the last node and append the new node
    slot *curr = slots_head;
    while (curr->next != NULL)
    {
      curr = curr->next;
    }
    curr->next = new_slot;
    return new_slot;
  }
}

static int device_open(struct inode *inode,
                       struct file *file)
{
  int minor = iminor(inode); // On opening a file we want to save the minor for future use
  data *fdata = (data *)kmalloc(sizeof(data), GFP_KERNEL); // Use the data struct to hold the minor of the file (and its channel in the future)
  if (fdata == NULL)
  {
    return -1;
  }
  fdata->minor = minor;
  fdata->file_channel = NULL;
  file->private_data = (void *)fdata;
  return SUCCESS;
}

static int device_release(struct inode *inode,
                          struct file *file)
{
  data *fdata = (data *)file->private_data; // When closing the file we no longer need the data so we free it.
  kfree(fdata);
  return SUCCESS;
}

static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset)
{
  int bytes_read = 0;
  int i;
  int j;
  data *fdata = (data *)file->private_data; 
  channel *chnl = fdata->file_channel; // Get the channel from the file
  char * backup = (char *)kmalloc(sizeof(char)*length, GFP_KERNEL); // Backup for the buffer in case of a failed read
  if (chnl == NULL)
  { // Check if such channel exists
    // errno = EINVAL;
    return -EINVAL;
  }
  if (chnl->msg_len == 0)
  { // Check if channel message not empty
    // errno = EWOULDBLOCK;
    return -EWOULDBLOCK;
  }
  if (length < chnl->msg_len)
  { // Check if provided length is less than the channel message length
    // errno = ENOSPC;
    return -ENOSPC;
  }
  for (i = 0; i < length; ++i)
  { // Copy the users message to our backup buffer
    if (get_user(backup[i], &buffer[i]) != 0)
    {
      // errno = EBADMSG;
      return -EBADMSG;
    }
  }
  for (i = 0; i < length && i < chnl->msg_len; i++)
  {
    if (put_user(chnl->msg[i], &buffer[i]) != 0)
    {
      for (j = 0; j < i; j++)
      { // Restore the buffer in case of fail
        put_user(backup[j], &buffer[j]); // Answer in forum said we can assume this succeedes
      }
      // errno = EBADMSG;
      return -EBADMSG;
    }
    bytes_read++;
  }
  kfree(backup);
  return bytes_read;
}
static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset)
{
  char the_message[BUF_LEN]; // We use a middle buffer to copy to so we can make sure the write was atomic
  int i;
  data *fdata = (data *)file->private_data;
  channel *chnl = fdata->file_channel; // Get the channel from the file
  if (file->private_data == NULL)
  { // Check if channel id exists
    // errno = EINVAL;
    return -EINVAL;
  }
  if (chnl == NULL)
  { // Check if such channel exists
    // errno = EINVAL;
    return -EINVAL;
  }

  if (length <= 0 || length > BUF_LEN)
  {
    // errno = EMSGSIZE;
    return -EMSGSIZE;
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  { // Copy the users message to our middle buffer
    if (get_user(the_message[i], &buffer[i]) != 0)
    {
      // errno = EBADMSG;
      return -EBADMSG;
    }
  }
  for (i = 0; i < length && i < BUF_LEN; ++i)
  { // Copy the users message to our channel buffer
    chnl->msg[i] = the_message[i];
  }
  chnl->msg_len = length;
  return chnl->msg_len;
}
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param)
{
  if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)
  {
    // errno = EINVAL;
    return -EINVAL;
  }
  data *fdata = (data *)file->private_data;
  int minor = fdata->minor;

  slot *curr = slots_head;
  while (curr->next != NULL)
  { // Get to the corresponding slot node
    if (curr->minor == minor)
    {
      break;
    }
    curr = curr->next;
  }
  channel *chnl_head = curr->channel_head;
  if (curr->channel_head == NULL)
  { // If no channels yet, create a new one
    channel *new_channel = insert_channel(&(curr->channel_head), ioctl_param);
    new_channel->id = ioctl_param;
    new_channel->msg_len = 0;
    fdata->file_channel = new_channel; // Link the created channel with the current file
    file->private_data = (void *)fdata;
    return SUCCESS;
  }
  while (chnl_head->next != NULL)
  { // If there are channels, look through the list for a channel with the same given ID
    if (chnl_head->id == ioctl_param)
    {
      fdata->file_channel = chnl_head; // Link the existing channel with the current file
      file->private_data = (void *)fdata;
      return SUCCESS;
    }
    chnl_head = chnl_head->next;
  }
  if (chnl_head->id == ioctl_param)
  {
    fdata->file_channel = chnl_head; // Link the existing channel with the current file
    file->private_data = (void *)fdata;
    return SUCCESS;
  }
  channel *new_channel = insert_channel(&(curr->channel_head), ioctl_param); // If we didn't find an existing channel, create a new one and add it
  new_channel->id = ioctl_param;
  new_channel->msg_len = 0;
  fdata->file_channel = new_channel; // Link the created channel with the current file
  file->private_data = (void *)fdata;
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
  for (i = 0; i < MAX_MINOR_NUM; i++) // We can have at most 256 minor numbers so we can just initialize the list now instead of initializing one by one later
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
  // Free the allocated memory (our slot and channel lists)
  slot *curr_slot = slots_head;
  slot *next_slot;
  while (curr_slot != NULL)
  {
    channel *curr_channel = curr_slot->channel_head;
    channel *next_channel;
    // Free the memory of the channel linked list
    while (curr_channel != NULL)
    {
      next_channel = curr_channel->next;
      kfree(curr_channel);
      curr_channel = next_channel;
    }

    next_slot = curr_slot->next;
    kfree(curr_slot);
    curr_slot = next_slot;
  }

  slots_head = NULL; // Reset the head node to NULL

  // Unregister the device
  // Should always succeed
  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
