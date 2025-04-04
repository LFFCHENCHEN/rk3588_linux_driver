#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#define IO_READ  0x00
#define IO_WRITE 0x01

int main(int argc, char **argv)
{
	int fd;
	int buf[2];

	if ((argc != 4) && (argc != 5))
	{
		printf("Usage: %s <dev> r <addr>\n", argv[0]);
		printf("       %s <dev> w <addr> <val>\n", argv[0]);
		return -1;
	}
	
	fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		printf(" can not open %s\n", argv[1]);
		return -1;
	}

	if (argv[2][0] == 'r')
	{
		buf[0] = strtoul(argv[3], NULL, 0);
		ioctl(fd, IO_READ, buf);
		printf("Read addr 0x%x, get data 0x%x\n", buf[0], buf[1]);
	}
	else
	{
		buf[0] = strtoul(argv[3], NULL, 0);
		buf[1] = strtoul(argv[4], NULL, 0);;
		ioctl(fd, IO_WRITE, buf);
	}
	
	return 0;
}