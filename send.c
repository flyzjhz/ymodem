#include 	 "ymodem.h"
#include <syslog.h>

unsigned char Eot_Send=0;           /*if control program has send "C" to request send,then it should be set to 1*/
unsigned char FirstPackSend=0;
unsigned char SOH_Idx=0;	        /*SOH_Idx:1 128 bytes; 2:1024 bytes(STX:02)*/
unsigned int blk_num_send=0;
unsigned char file_send_end=0;       /*indicate the file has been sended*/
unsigned char file_send_end_ACK=0;   /*indicate the file has been sended and receiver responsed*/
unsigned char file_name_send_ACK=0;  /*indicate the file name packet has been sended*/
unsigned char FirstSend=1;           /*indicate the file name packet start to be sended*/
unsigned char PacketStartFlagSend=0; /*indicate the file content packet start to be sended*/
unsigned char NoWait=0;              /*if get 'G' command, no wait the next cmd*/
unsigned char No_More_File=0;        /*indicate all the files has been sended*/
unsigned char Empty_Ind=0;

unsigned int NumBlks;	             /*the number of blocks of the file*/
unsigned int LenLastBlk;             /*length of the last block*/
unsigned char SOHLastBlk;            /*the SOH ind of the last block*/

int get_filelen(char* file_name);
char* get_file_name(char* file_name);
int real_send(char* file_name);
int procsend(char* send_buf,char* file_name);
int Procpacket(char* send_buf,char* file_name);


extern unsigned int brate;

int control_send(char* file_name)
{
	char send_next;
	int send_idx;

	char cmd_ind;
	int len;
	int CAN_CNT=0;
	int CAN_CNT1=0;
    static int writeerr = 0;

#define MAX_INVALID_REQ     3

    static int inval_req = 0;// invalid request 

    fd = open_USB();
    
    if(fd == -1) {
        syslog(LOG_ERR, "[%s %d] Open USB failed\n", __FUNCTION__, __LINE__);
        return(COM_OPEN_ERR);
    } else {
        syslog(LOG_INFO, "[%s %d] Open USB succeed\n", __FUNCTION__, __LINE__);
    }

    syslog(LOG_INFO, "[%s %d] Ready to send file file_name %s\n", __FUNCTION__, __LINE__, file_name);
    //tcflush(fd, TCIOFLUSH);
	for(;;)
	{	
		if((len=_read(fd,&cmd_ind,1))==-1)
        {
        	if(errno==EAGAIN) {
				continue;
        	}
            
            _close(fd);
		
			return(COM_READ_ERR);
        }

        syslog(LOG_INFO, "\n[%s %d] cmd_ind 0x%x\n", __FUNCTION__, __LINE__, cmd_ind);
		switch(cmd_ind)
		{
			case 'C':
				if(CAN_CNT)
					CAN_CNT=0;
				if(FirstSend|file_name_send_ACK|file_send_end_ACK)
				{
					if(file_send_end_ACK)
					{
						Empty_Ind=1;
					}
 					CrcFlag=1;
					break;
				}
				else
				{
					syslog(LOG_INFO, "[%s %d]unvalid request:0x%x!\n",
                                __FUNCTION__, __LINE__, cmd_ind);
					continue;
				}
			case 'G':
				if(CAN_CNT)
					CAN_CNT=0;
                syslog(LOG_INFO, "[%s %d]\n", __FUNCTION__, __LINE__);
				if(PacketStartFlagSend)
				{
					FirstPackSend=0; //indicate filename has been sended successfully.
					file_name_send_ACK=1;
					NoWait=1;
					break;
				}
				if(FirstSend|file_send_end_ACK)
				{
					if(file_send_end_ACK)
					{
						Empty_Ind=1;
					}
 					CrcFlag=1;
                    syslog(LOG_INFO, "[%s %d]\n", __FUNCTION__, __LINE__);
					break;
				}
				else
				{
					syslog(LOG_INFO, "unvalid request:0x%x!\n",cmd_ind);
					continue;
				}

			case NAK:
				if(CAN_CNT)
					CAN_CNT=0;
				if(FirstPackSend)
				{
					FirstSend=1;
					PacketStartFlagSend=0;
					blk_num_send-=1; //resend filename packet
					break;
				}
				if(file_name_send_ACK|FirstSend)
					CrcFlag=0;
				else if(PacketStartFlagSend)
				{
					syslog(LOG_INFO, "unvalid request:0x%x!\n",cmd_ind);
					continue;
				}
				else
				{
					blk_num_send-=1; //ressend data packet
					break;
				}
				
			case ACK:
				if(CAN_CNT)
					CAN_CNT=0;
				if(FirstSend)
				{
					syslog(LOG_INFO, "unvalid request!:0x%x\n",cmd_ind);
					continue;

				}
				if(PacketStartFlagSend)
				{
					FirstPackSend=0; //indicate filename has been sended successfully.
					file_name_send_ACK=1;

					continue;
				}
				if(Eot_Send)
					file_send_end_ACK=1;
				break;
			case CAN:
			{
				CAN_CNT++;
				if(CAN_CNT==2)
				{
					syslog(LOG_INFO, "[%s %d]the transmission is canceled by user!\n", __FUNCTION__, __LINE__);
					_close(fd);
					exit(0);
				}
				continue;
			}
				
			default:
			{
				if(CAN_CNT)
					CAN_CNT=0;
                inval_req++;
				syslog(LOG_INFO, "unvalid request!:0x%x\n",cmd_ind);
                if (inval_req <= MAX_INVALID_REQ)
    				continue;
                else 
                    return COM_READ_ERR;
			}
		}
		send_next=1;
		for(;send_next;)
		{
			send_next=0;
			if(NoWait)
			{
				send_next=1;
                syslog(LOG_INFO, "[%s %d]\n", __FUNCTION__, __LINE__);    
				if((len=_read(fd,&cmd_ind,1))==-1)
        		{
        			if(errno!=EAGAIN)
        			{
           			 	_close(fd);
						return(COM_READ_ERR);
					}
        		}
        		if(len)
        		{
        			if(cmd_ind==CAN)
        			{
        				CAN_CNT1++;
        				if(CAN_CNT1==2)
        				{
        					syslog(LOG_INFO, "the transmission is canceled by user!\n");
							_close(fd);
							exit(0);
        				}
        			}
        			else
        			{
        				CAN_CNT1=0;
        			}
        		}
			}

			if(!Empty_Ind)
			{
				if(file_send_end)//send "EOT" to indicate end
				{
SEND_END:                       
					cmd_ind=(char)(EOT);
					if(_write(fd,&cmd_ind,1)==-1)
					{
                        syslog(LOG_INFO, "[%s %d]\n", __FUNCTION__, __LINE__);
                        if (writeerr < 64) {
                            syslog(LOG_INFO, "[%s %d]Send END write err! %s\n", 
                                __FUNCTION__, __LINE__, strerror(errno));
                            fd = open_USB();
                            goto SEND_END;
                        } else {
                            syslog(LOG_INFO, "[%s %d]Send END write err over 64 times! %s\n", 
                                __FUNCTION__, __LINE__, strerror(errno));
                            return COM_WRITE_ERR;
                        }
					}                    
                    
					Eot_Send=1;
					if(NoWait)
						send_next=0;	
					continue;
				}
			}
            
			send_idx=real_send(file_name);	//transmit the data packet
            
			if(send_idx==ALL_NEED_OFFSET)//just one file.if more files ,need modify
			{
				continue;
			} 
            else if(send_idx!=0)
			{
				_close(fd);
				return(send_idx);//error occured while sending
			}
			if(PacketStartFlagSend) {//used to know the "NAK" indicate re-send or just checksum
				PacketStartFlagSend=0;
            }
			if(FirstSend)
			{
				FirstSend=0;//never resend file name packet?    
				PacketStartFlagSend=1;
			}
		}
	}
	
}

int real_send(char* file_name)
{
	char send_buf[1029]; //max value:1(SOH)+1(Blk#)+1(255-Blk#)+1024(data)+2(crc)
	int frame_idx;
	unsigned int packet_len;
	unsigned int i;
    static int writeerr = 0;

	if(Empty_Ind)
		blk_num_send=0;
	else if(!FirstSend)
		blk_num_send++;

	frame_idx=procsend(send_buf,file_name);	
	if(frame_idx)
	{
		return(frame_idx);//error occured
	}
	if(SOH_Idx==1)
		packet_len=132;
	else
		packet_len=1028;
		
	if(CrcFlag)
		packet_len+=1;

    //printf("SOH_Idx %d packet_len %d\n", SOH_Idx, packet_len);
    //printf("%x %x %x\n", send_buf[0], send_buf[1],send_buf[2]);
	for(i=0;i<packet_len;i++)
	{
WRITE_ERR:
		if(_write(fd,send_buf+i,1)==-1)
		{   
            if (writeerr < 64) {
       			syslog(LOG_INFO, "[%s %d]data write err! %s\n", 
                        __FUNCTION__, __LINE__, strerror(errno));

                fd = open_USB();
                writeerr++;
                goto WRITE_ERR;
            } else {
                syslog(LOG_ERR, "[%s %d]data write err over 64 times ! %s\n", 
                        __FUNCTION__, __LINE__, strerror(errno));
                return COM_WRITE_ERR;
            }
		}
	}
	if(No_More_File)
	{
		delay(10000000);
        close(fd);
        syslog(LOG_INFO, "[%s %d] exit\n", __FUNCTION__, __LINE__);
		exit(0);
	}
		
	if(blk_num_send==NumBlks)//the last block has been sended successful.
	{
		file_send_end=1;
		return(ALL_NEED_OFFSET);//发送完一个文件，所有参数复位继续发送下一个文件
	}	
	
	return 0;

}
/***************************************************************/
/***************************************************************/
int procsend(char* send_buf,char* file_name)
{
	unsigned int blklen,i;

	char* filename_real;
	unsigned int size_rec;

	unsigned short oldcrc=0;
	unsigned char checksum=0;

	int get_filelenind;

	char packet_ind;
	
	if(FirstSend)/*file name packet*/
	{
		get_filelenind=get_filelen(file_name);
		if(get_filelenind)
			return(get_filelenind);
			
		LenLastBlk=FileLen%1024;
		NumBlks=(FileLen>>10);

        //printf("[%s %d]FileLen %d, NumBlks %d, LenLastBlk %d\n", 
            //        __FUNCTION__, __LINE__,FileLen, NumBlks, LenLastBlk);
		
		filename_real=get_file_name(file_name);//malloc a space,need be freed
		if((filename_real==(char*)FILE_UNVILID)||(filename_real==(char*)MALLOC_ERROR))
			exit((int)filename_real);
			
		if(LenLastBlk)
		{
			NumBlks+=1;
           // printf("[%s %d] NumBlks %d, LenLastBlk %d\n", __FUNCTION__, __LINE__,NumBlks, LenLastBlk);
			if(LenLastBlk<128)
			{
				SOHLastBlk=1;
                //printf("[%s %d] SOHLastBlk %d, LenLastBlk %d\n", __FUNCTION__, __LINE__,SOHLastBlk, LenLastBlk);

			}
			else 
			{
                SOHLastBlk=2;
                //printf("[%s %d] SOHLastBlk %d, LenLastBlk %d\n", __FUNCTION__, __LINE__,SOHLastBlk, LenLastBlk);
			}

		}
		else
		{
			LenLastBlk=1024;
		}
		if(strlen(filename_real)>128)
		{
			SOH_Idx=2;
			blklen=1024;
		}
		else
		{
			SOH_Idx=1;
			blklen=128;
		}

		if(CrcFlag)
			memset(send_buf,'\0',blklen+5);
		else
			memset(send_buf,'\0',blklen+4); //sumcheck
		
		send_buf[0]=SOH_Idx;
		send_buf[1]=0;
		send_buf[2]=(char)0xFF;

       // printf("[%s %d]send_buf[0] %x\n", __FUNCTION__, __LINE__, send_buf[0]);
        
		size_rec=strlen(filename_real);
		strncpy(send_buf+3,filename_real,size_rec);
		
		size_rec+=4; //move to the position to packet file length
		
		sprintf(send_buf+size_rec,"%d",FileLen);
		
		/************reserved for crc or sumcheck**************/
		for (i=0;i<blklen;i++ ) 
		{
			oldcrc=updcrc((0377& send_buf[i+3]), oldcrc);
			checksum += send_buf[i+3];
		}
		if(CrcFlag)
		{
			oldcrc=updcrc(0,updcrc(0,oldcrc));
			send_buf[blklen+3]=(oldcrc>>8);
			send_buf[blklen+4]=(oldcrc&0xFF);
		}
		else
		{
			send_buf[blklen+3]=checksum;
		}

		free(filename_real);
		FirstPackSend=1;
		return 0;
	}
	else
	{
		if(blk_num_send<=NumBlks)
		{
			if(Empty_Ind) //send empty packet
				SOH_Idx=1;
			else
				SOH_Idx=2;
            //printf("[%s %d]---- 111 SOH_Idx %d\n", __FUNCTION__, __LINE__, SOH_Idx);
			packet_ind=Procpacket(send_buf,file_name);
            //printf("[%s %d]---- 222 SOH_Idx %d\n", __FUNCTION__, __LINE__,  SOH_Idx);
		}

        //printf("[%s %d] packet_ind %d\n", __FUNCTION__, __LINE__, packet_ind);
        //printf("[%s %d]send_buf[0] %x\n", __FUNCTION__, __LINE__, send_buf[0]);
		return(packet_ind);
	}
}


/***************************************************************/
/***************************************************************/
int Procpacket(char* send_buf,char* file_name)
{
	//FILE* fp_send;
	int fd_send;
	unsigned int size_read;
	unsigned int blklen;
	unsigned short oldcrc=0;
	unsigned char checksum=0;
	unsigned int i;
	
	if((fd_send=_open(file_name,_O_RDONLY|_O_BINARY))==-1)
	{
		syslog(LOG_ERR, "[%s %d]can't open file %s for read!\n", __FUNCTION__, __LINE__, file_name);
		return(FILE_OPEN_ERR);
	}

    //printf("[%s %d] SOH_Idx %d\n", __FUNCTION__, __LINE__, SOH_Idx);
	send_buf[0]=SOH_Idx;
	
	send_buf[1]=(blk_num_send&0xFF);
	send_buf[2]=255-blk_num_send;


    //printf("[%s %d]blk_num_send %d,NumBlks %d\n",  
           // __FUNCTION__, __LINE__, blk_num_send, NumBlks);
	if(blk_num_send==NumBlks)
	{
		SOH_Idx=SOHLastBlk;
		size_read=LenLastBlk;
		if(SOH_Idx==1)
			blklen=128;
		else
			blklen=1024;
        
        //printf("[%s %d] SOH_Idx %d, blklen %d, SOHLastBlk %d\n", 
         //       __FUNCTION__, __LINE__, SOH_Idx, blklen, SOHLastBlk);
	}
	else
	{
		if(SOH_Idx ==1 )
			size_read=128;
		else
			size_read=1024;	//STX
		blklen=size_read;
        //printf("[%s %d] SOH_Idx %d, blklen %d\n", __FUNCTION__, __LINE__, SOH_Idx, blklen);
	}

    send_buf[0] = SOH_Idx;
	//printf("[%s %d] SOH_Idx %d, sendbuf[0] %d\n", __FUNCTION__, __LINE__, SOH_Idx, send_buf[0]);

	if(!file_send_end)
	{
		_lseek(fd_send,FileLenBkup-FileLen,SEEK_SET);
		//data part

		if(_read(fd_send,send_buf+3,size_read)==-1)
		{
			return(FILE_READ_ERR);
		}
	}
	FileLen-=size_read;

	_close(fd_send);
	
	if(Empty_Ind)
	{
		memset(send_buf+3,'\0',blklen); 
		No_More_File=1;  //now only for one file
	}

	//filled the last block with CPMEOF
	if(blk_num_send==NumBlks)
		memset(send_buf+3+size_read,CPMEOF,blklen-size_read);
	
	for (i=0;i<blklen;i++ ) 
	{
		oldcrc=updcrc((0377& send_buf[i+3]), oldcrc);
		checksum += send_buf[i+3];
	}
	
	//CRC and checksum part.
	if(CrcFlag)
	{
		oldcrc=updcrc(0,updcrc(0,oldcrc));
		send_buf[blklen+3]=(oldcrc>>8);
		send_buf[blklen+4]=(oldcrc&0xFF);
	}
	else
	{
		send_buf[blklen+3]=checksum;
	}

    //printf("[%s %d]send_buf[0] %d\n", __FUNCTION__, __LINE__, send_buf[0]);
	return 0;
}


/***************************************************************/
/***************************************************************/
int get_filelen(char* file_name)
{
	FILE* fp_send;
	int c;
	
	if( (fp_send = fopen( file_name, "rb" )) == NULL )
	{
		syslog(LOG_ERR, "can't open file %s for read!\n",file_name);
		return(FILE_OPEN_ERR);
	}
	
	/* Cycle until end of file reached: */

	while((c=fgetc(fp_send))!=EOF)
		FileLen++;

	fclose( fp_send );
	
	FileLenBkup=FileLen;
	return(0);
}


/***************************************************************/
/***************************************************************/
/*need be freed*/
char* get_file_name(char* file_name)
{
	unsigned int real_name_len=0,name_len;
	unsigned int i;
	char* filename_real;
	char seperater;

#ifdef WINYWY
	seperater='\\';
#else
	seperater='/';
#endif
	name_len=strlen(file_name);
	
	for(i=1;i<name_len+1;i++)
	{
		if(*(file_name+(name_len-i))!=seperater)
		{
			real_name_len++;
		}
		else
		{
			break;
		}
	}
	
	if(!real_name_len)
	{
		return((char*)FILE_UNVILID);
	}	
	
	if((filename_real=(char*)malloc(real_name_len+1))==NULL)
	{
		return((char*)MALLOC_ERROR);
	}

	for(i=1;i<real_name_len+1;i++)
	{
		filename_real[real_name_len-i]=*(file_name+(name_len-i));
	}
	filename_real[real_name_len]='\0';
	
	return(filename_real);
}

