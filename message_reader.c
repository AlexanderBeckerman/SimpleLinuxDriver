#include "message_slot.h"

#include <fcntl.h>     /* open */
#include <unistd.h>    /* exit */
#include <sys/ioctl.h> /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int file_desc;
    int ret_val;
    char *file_path;
    int channel_id;
    int bytes_read;
    char message[BUF_LEN];

    if (argc != 3)
    {
        perror("invalid number of arguments\n");
        exit(1);
    }
    file_path = argv[1];
    channel_id = atoi(argv[2]);
    file_desc = open(file_path, O_RDWR);
    if (file_desc < 0)
    {
        perror("Can't open device file\n");
        exit(1);
    }
    ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
    if (ret_val == -1)
    {
        perror("error occured in ioctl\n");
        exit(1);
    }
    bytes_read = read(file_desc, message, BUF_LEN);
    if (bytes_read < 0)
    {
        perror("error reading from device\n");
        exit(1);
    }
    ret_val = close(file_desc);
    if (ret_val < 0)
    {
        perror("error closing file\n");
        exit(1);
    }
    ret_val = write(STDOUT_FILENO, message, bytes_read);
    if (ret_val != bytes_read)
    {
        perror("error occured writing to stdout\n");
        exit(1);
    }

    exit(0);
}
