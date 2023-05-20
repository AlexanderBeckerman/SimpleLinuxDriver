#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

#define MAJOR_NUM 235
#define SUCCESS 0
#define BUF_LEN 128
#define DEVICE_RANGE_NAME "char_dev"
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#endif