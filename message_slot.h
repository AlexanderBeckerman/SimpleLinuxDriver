#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235
#define MAX_MINOR_NUM 257
#define SUCCESS 0
#define BUF_LEN 128
#define DEVICE_RANGE_NAME "char_dev"
#define DEVICE_FILE_NAME "message_slot"
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#endif