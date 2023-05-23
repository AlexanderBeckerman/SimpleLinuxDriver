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
    char *message;

    if (argc != 4)
    {
        perror("invalid number of arguments\n");
        exit(1);
    }
    file_path = argv[1];
    channel_id = atoi(argv[2]);
    message = argv[3];

    file_desc = open(file_path, O_RDWR);
    if (file_desc < 0)
    {
        printf("Can't open device file: %s\n", file_path);
        exit(1);
    }

    ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
    if (ret_val == -1)
    {
        perror("error occured in ioctl\n");
        exit(1);
    }
    ret_val = write(file_desc, message, strlen(message)); // strlen does not count the null character
    if (ret_val == -1)
    {
        perror("error writing to device\n");
        exit(1);
    }
    ret_val = close(file_desc);
    if (ret_val < 0){
        perror("error closing file\n");
        exit(1);
    }
    exit(0);
}
