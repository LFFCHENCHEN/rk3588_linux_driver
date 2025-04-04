#include <stdio.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/input/event11"
const char *event_type(int type) // Renamed from evebt_type to event_type
{
    switch (type)
    {
    case EV_ABS:
        return "EV_ABS";
    case EV_KEY:
        return "EV_KEY";
    case EV_REL:
        return "EV_REL";
    case EV_SYN:
        return "EV_SYN";
    default:
        return "UNKNOWN";
    }
}

int main(int argc,char *argv[])
{
    int fd =open(DEVICE_PATH, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return -1;
    }
    struct input_event ev;
    while(1)
    {
        ssize_t n=read(fd, &ev, sizeof(ev));
        if (n!=sizeof(ev))
        {
            perror("read");
            break;
        }
        printf("[时间] %ld.%06ld\t", ev.time.tv_sec, ev.time.tv_usec);
        printf("[类型] %s(0x%04x)\t", event_type(ev.type), ev.type); // Function call remains unchanged
        printf("[代码] 0x%04x\t", ev.code);
        printf("[数值] %d\n", ev.value);
    }
    close(fd);
    return 0;
}