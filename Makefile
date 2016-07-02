CC=$(TOOLPREFIX)gcc

CFLAGS= -c -fPIC
INCLUDE= -I$(TOPDIR)/public
LIBPATH= -L$(TOPDIR)/apps/ap_server/implement/libs -L$(INSTALL_ROOT)/lib

OBJS=ymodem.c serialio.c send.c receive.c 
#OBJS=shmodel.c serialio.c 

	 
all: ymodem
	echo $(IMPLEMENTPATH)
	
ymodem: 
	$(CC) -o ymodem $(INCLUDE) $(LIBPATH) $(OBJS) -lm
	
clean:
	rm -f ymodem
	rm -f *.o