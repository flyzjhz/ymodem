#include "serialio.h"
#include <syslog.h>
#include <dlfcn.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include "ymodem.h" 

int fd;

char CrcFlag=0;
unsigned int FileLen=0;
unsigned int FileLenBkup=0;

#define SHUPGRADE  0

int open_USB()
{
	int fd = -1;
    struct termios tty;
    
	while(1)
	{
		fd = open("/dev/ttyUSB0",O_RDWR | O_NOCTTY);
		if(fd < 0)
		{
			fd = open("/dev/ttyUSB1",O_RDWR | O_NOCTTY);
		}
		
		if(fd > 0)
		{
			break;
		}
		syslog(LOG_ERR, "[%s %d] %s",
                __FUNCTION__, __LINE__, strerror(errno));
        
		sleep(1);
	}
//	setParams(fd, brate, 'N', 8, 1, 0, 0);

    tcgetattr(fd, &tty);

#if 1
    cfsetispeed(&tty, B38400);     //38400Bps
	cfsetospeed(&tty, B38400);     //38400Bps
#else
	cfsetispeed(&tty, B115200);     //38400Bps
	cfsetospeed(&tty, B115200);     //38400Bps
#endif

    tty.c_lflag &= ~ICANON;
	tty.c_lflag &= ~IEXTEN;
	tty.c_lflag &= ~ISIG;
	tty.c_lflag &= ~ECHO;

	tty.c_iflag = 0;
	
	tty.c_oflag = 0;


	if (tcsetattr(fd,TCSANOW,&tty) != 0)   
	{ 
		return (COM_SET_ERR);  
	}

	return fd;
}


int main(int argc,char* argv[])
{
	int i;
	char bt;
	char* fp;
	int trans_ind;
	int set_ind;
    char device[64] = {0};

    strncpy(device, "/dev/ttyS0", strlen("/dev/ttyS0"));
    if (argc < 2) {
        printf("[usage]:\n%s r\n%s s filename\n", argv[0], argv[0]);
        exit(0);
    }

	switch(*argv[1])
	{
		case 'r':
			trans_ind=control_recv(device);
			break;
		case 's':
			fp=argv[2];
			if(fp==NULL)
			{
				printf("please input file name!\n");
				exit(1);
			}
			trans_ind=control_send(fp);
			break;
		default :
			printf("please use 'r' or 's fname' cmd to receive or send file\n");
			return 0;
	}

	if(trans_ind){
		syslog(LOG_ERR, "[ymodem]sorry! get error in transmission!ERR ID:0x%x\n",trans_ind);
		if(trans_ind==COM_OPEN_ERR)
		{
			syslog(LOG_ERR, "sorry,I can't open the serial port!\n");
            exit(1);
		} else if (trans_ind == COM_READ_ERR) {
            syslog(LOG_ERR, "sorry, received too many invalid request\n");
        } else {
			if((fd=_open(device, _O_RDWR|_O_BINARY|_O_NONBLOCK ))==-1)
			{
				syslog(LOG_ERR, "sorry,I can't open the serial port!\n");
			}
			else
			{
				bt=(char)CAN;
				if(_write(fd, &bt,1)==-1) //request sender to send file with CRC check
				{
					_close(fd);
					exit(1);
				}
				if(_write(fd, &bt,1)==-1) //request sender to send file with CRC check
				{
					_close(fd);
					exit(1);
				}
				delay(10000000);
			}
			_close(fd);
			exit(1);
		}
		
	}
	return 0;

}

void delay(int clock_count)
{
	int i;
	for(i=0;i<clock_count;i++)  //delay
	{
	}
}
