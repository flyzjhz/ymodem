//#include <stropts.h>
#include <stdlib.h>
#include <unistd.h>
#include "serialio.h"

/****************************
 * serialio.cpp£ ´®¿ÚÍ¨ÓÃAPI*
 * **************************/

/** Set hardware flow control. 
 */
void setHardwareFlow(int fd, int on) {
#ifdef _DGUX_SOURCE
	struct termiox x;
#endif
#ifdef POSIX_TERMIOS
	struct termios tty;
#endif

#ifdef POSIX_TERMIOS
	tcgetattr(fd, &tty);
	if (on)
		tty.c_cflag |= CRTSCTS;
	else
		tty.c_cflag &= ~CRTSCTS;
	tcsetattr(fd, TCSANOW, &tty);
#endif

#ifdef _DGUX_SOURCE
	if (ioctl(fd, TCGETX, &x) < 0) {
		fprintf(stderr, _("can't get termiox attr.\n"));
		return;
	}
	x.x_hflag = on ? RTSXOFF|CTSXON : 0;

	if (ioctl(fd, TCSETX, &x) < 0) {
		fprintf(stderr, _("can't set termiox attr.\n"));
		return;
	}
#endif
}

/* Set RTS line. Sometimes dropped. Linux specific? */
	void setRTS(int fd) {
#if defined(TIOCM_RTS) && defined(TIOCMODG)
		{
			int mcs=0;

			ioctl(fd, TIOCMODG, &mcs);
			mcs |= TIOCM_RTS;
			ioctl(fd, TIOCMODS, &mcs);
		}
#endif
#ifdef _COHERENT
		ioctl(fd, TIOCSRTS, 0);
#endif
	}

/*
 * Drop DTR line and raise it again.
 */
void DTRToggle(int fd, int sec) {
	{
#ifdef TIOCSDTR
		/* Use the ioctls meant for this type of thing. */
		ioctl(fd, TIOCCDTR, 0);
		if (sec>0) {
			sleep(sec);
			ioctl(fd, TIOCSDTR, 0);
		}

#else /* TIOCSDTR */
#  if defined (POSIX_TERMIOS) && !defined(_HPUX_SOURCE)

		/* Posix - set baudrate to 0 and back */
		struct termios tty, old;

		tcgetattr(fd, &tty);
		tcgetattr(fd, &old);
		cfsetospeed(&tty, B0);
		cfsetispeed(&tty, B0);
		tcsetattr(fd, TCSANOW, &tty);
		if (sec>0) {
			sleep(sec);
			tcsetattr(fd, TCSANOW, &old);
		}

#  else /* POSIX */
#    ifdef _V7

		/* Just drop speed to 0 and back to normal again */
		struct sgttyb sg, ng;

		ioctl(fd, TIOCGETP, &sg);
		ioctl(fd, TIOCGETP, &ng);

		ng.sg_ispeed = ng.sg_ospeed = 0;
		ioctl(fd, TIOCSETP, &ng);
		if (sec>0) {
			sleep(sec);
			ioctl(fd, TIOCSETP, &sg);
		}

#    endif /* _V7 */
#    ifdef _HPUX_SOURCE
		unsigned long mflag = 0L;

		ioctl(fd, MCSETAF, &mflag);
		ioctl(fd, MCGETA, &mflag);
		mflag = MRTS | MDTR;
		if (sec>0) {
			sleep(sec);
			ioctl(fd, MCSETAF, &mflag);
		}
#    endif /* _HPUX_SOURCE */
#  endif /* POSIX */
#endif /* TIOCSDTR */
	}
}

/*
 * Send a break
 */
void sendBreak(int fd) { 
#ifdef POSIX_TERMIOS
	tcsendbreak(fd, 0);
#else
#  ifdef _V7
#    ifndef TIOCSBRK
	{
		struct sgttyb sg, ng;

		ioctl(fd, TIOCGETP, &sg);
		ioctl(fd, TIOCGETP, &ng);
		ng.sg_ispeed = ng.sg_ospeed = B110;
		ng.sg_flags = BITS8 | RAW;
		ioctl(fd, TIOCSETP, &ng);
		write(fd, "\0\0\0\0\0\0\0\0\0\0", 10);
		ioctl(fd, TIOCSETP, &sg);
	}
#    else
	ioctl(fd, TIOCSBRK, 0);
	sleep(1);
	ioctl(fd, TIOCCBRK, 0);
#    endif
#  endif
#endif
}

/*
 * Get the dcd status
 */
int getDCD(int fd)
{
#ifdef TIOCMODG
	{
		int mcs=0;

		ioctl(fd, TIOCMODG, &mcs);
		return(mcs & TIOCM_CAR ? 1 : 0);
	}
#else
	(void)fd;
	return(0); /* Impossible!! (eg. Coherent) */
#endif
}

/* Variables to save states in */
#ifdef POSIX_TERMIOS
static struct termios savetty;
static int m_word;
#else
static struct sgttyb savetty;
static struct tchars savetty2;
#  if defined (_BSD43) || defined (_V7)
static struct sgttyb sg;
static struct tchars tch;
static int lsw;
static int m_word;
#  endif
#endif

/*
 * Save the state of a port
 */
void saveState(int fd) {
#ifdef POSIX_TERMIOS
	tcgetattr(fd, &savetty);
#else
#  if defined(_BSD43) || defined(_V7)
	ioctl(fd, TIOCGETP, &sg);
	ioctl(fd, TIOCGETC, &tch);
#  endif
#  ifdef _BSD43
	ioctl(fd, TIOCLGET, &lsw);
#  endif
#endif
#ifdef TIOCMODG
	ioctl(fd, TIOCMODG, &m_word);
#endif
}

/*
 * Restore the state of a port
 */
void restoreState(int fd) {
#ifdef POSIX_TERMIOS
	tcsetattr(fd, TCSANOW, &savetty);
#else
#  if defined(_BSD43) || defined(_V7)
	ioctl(fd, TIOCSETP, &sg);
	ioctl(fd, TIOCSETC, &tch);
#  endif
#  ifdef _BSD43  
	ioctl(fd, TIOCLSET, &lsw);
#  endif
#endif
#ifdef TIOCMODS
	ioctl(fd, TIOCMODS, &m_word);
#endif
}

/*
 * Set the line status so that it will not kill our process
 * if the line hangs up.
 */
/*ARGSUSED*/ 
void noHang(int fd) {
	{
#ifdef POSIX_TERMIOS
		struct termios sgg;

		tcgetattr(fd, &sgg);
		sgg.c_cflag |= CLOCAL;
		tcsetattr(fd, TCSANOW, &sgg);
#else
#  if defined (_BSD43) && defined(LNOHANG)
		int lsw;

		ioctl(fd, TIOCLGET, &lsw);
		lsw |= LNOHANG;
		ioctl(fd, TIOCLSET, &lsw);
#  endif
#  ifdef _COHERENT
		/* Doesn't know about this either, me thinks. */
#  endif
#endif
	}
}

/*
 * Set hangup on close on/off.
 */
void hupOnClose(int fd, int on) {
	/* Eh, I don't know how to do this under BSD (yet..) */
#ifdef POSIX_TERMIOS
	{
		struct termios sgg;

		tcgetattr(fd, &sgg);
		if (on)
			sgg.c_cflag |= HUPCL;
		else
			sgg.c_cflag &= ~HUPCL;
		tcsetattr(fd, TCSANOW, &sgg);
	}
#endif
}

/*
 * See if there is input waiting.
 * returns: 0=nothing, >0=something, -1=can't.
 */
int readCheck(int fd) {
#ifdef FIONREAD
	long i = -1;

	(void) ioctl(fd, FIONREAD, &i);
	return((int)i);
#else
#  if defined(F_GETFL) && defined(O_NDELAY)
	int i, old;
	char c;

	old = fcntl(fd, F_GETFL, 0);
	(void) fcntl(fd, F_SETFL, old | O_NDELAY);
	i = read(fd, &c, 1);
	(void) fcntl(fd, F_SETFL, old);

	return(i);
#  else
	return(-1);
#  endif
#endif
}

/*
 * Get maximum speed.
 * Returns maximum speed / 100. (192-1152)
 */
int getMaxSpeed() {
#ifdef B230400
	return(2304);
#elif defined(B115200)
	return(1152);
#elif defined(B57600)
	return(576);
#elif defined(B38400)
	return(384);
#elif defined(EXTB)
	return(384);
#elif defined(B19200)
	return(192);
#elif defined(EXTA)
	return(192);
#else
	return(96);
#endif
}

/*
 * Set baudrate, parity and number of bits.
 */
int setParams(int fd, long baudr, char par, int bit, int stopb, int hwf, int swf) {
	int spd = -1;
	int newbaud;
    unsigned int ret = -1;
#ifdef POSIX_TERMIOS
	struct termios tty;
#else /* POSIX_TERMIOS */
	struct sgttyb tty;
#endif /* POSIX_TERMIOS */

#ifdef POSIX_TERMIOS
	tcgetattr(fd, &tty);
#else /* POSIX_TERMIOS */
	ioctl(fd, TIOCGETP, &tty);
#endif /* POSIX_TERMIOS */


	/* We generate mark and space parity ourself. */
	if (bit == 7 && (par == 'M' || par == 'S'))
		bit = 8;

	/* Check if 'baudr' is really a number */
	if ((newbaud = (baudr / 100)) == 0)
		newbaud = -1;

	switch(newbaud) {
		case 0:
#ifdef B0
			spd = B0;	break;
#else
			spd = 0;	break;
#endif
		case 3:		spd = B300;	break;
		case 6:		spd = B600;	break;
		case 12:	spd = B1200;	break;
		case 24:	spd = B2400;	break;
		case 48:	spd = B4800;	break;
		case 96:	spd = B9600;	break;
#ifdef B19200
		case 192:	spd = B19200;	break;
#else /* B19200 */
#  ifdef EXTA
		case 192:	spd = EXTA;	break;
#   else /* EXTA */
		case 192:	spd = B9600;	break;
#   endif /* EXTA */
#endif	 /* B19200 */
#ifdef B38400
		case 384:	spd = B38400;	break;
#else /* B38400 */
#  ifdef EXTB
		case 384:	spd = EXTB;	break;
#   else /* EXTB */
		case 384:	spd = B9600;	break;
#   endif /* EXTB */
#endif	 /* B38400 */
#ifdef B57600
		case 576:	spd = B57600;	break;
#endif
#ifdef B115200
		case 1152:	spd = B115200;	break;
#endif
#ifdef B230400
		case 2304:	spd = B230400;	break;
#endif
	}

#if defined (_BSD43) && !defined(POSIX_TERMIOS)
	if (spd != -1) tty.sg_ispeed = tty.sg_ospeed = spd;
	/* Number of bits is ignored */

	tty.sg_flags = RAW | TANDEM;
	if (par == 'E')
		tty.sg_flags |= EVENP;
	else if (par == 'O')
		tty.sg_flags |= ODDP;
	else
		tty.sg_flags |= PASS8 | ANYP;

	ioctl(fd, TIOCSETP, &tty);

#  ifdef TIOCSDTR
	/* FIXME: huh? - MvS */
	ioctl(fd, TIOCSDTR, 0);
#  endif
#endif /* _BSD43 && !POSIX_TERMIOS */

#if defined (_V7) && !defined(POSIX_TERMIOS)
	if (spd != -1) tty.sg_ispeed = tty.sg_ospeed = spd;
	tty.sg_flags = RAW;
	if (par == 'E')
		tty.sg_flags |= EVENP;
	else if (par == 'O')
		tty.sg_flags |= ODDP;

	ioctl(fd, TIOCSETP, &tty);
#endif /* _V7 && !POSIX */

#ifdef POSIX_TERMIOS

	if (spd != -1) {
		cfsetospeed(&tty, (speed_t)spd);
		cfsetispeed(&tty, (speed_t)spd);
	}

	switch (bit) {
		case 5:
			tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS5;
			break;
		case 6:
			tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS6;
			break;
		case 7:
			tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;
			break;
		case '8':
		default:
			tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
			break;
	}		
	/* Set into raw, no echo mode */
	tty.c_iflag =  IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cflag |= CLOCAL | CREAD;
#ifdef _DCDFLOW
	tty.c_cflag &= ~CRTSCTS;
#endif
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 5;

	if (swf)
		tty.c_iflag |= IXON | IXOFF;
	else
		tty.c_iflag &= ~(IXON|IXOFF|IXANY);

	tty.c_cflag &= ~(PARENB | PARODD);
	if (par == 'E')
		tty.c_cflag |= PARENB;
	else if (par == 'O')
		tty.c_cflag |= (PARENB | PARODD);

	if (stopb == 2)
		tty.c_cflag |= CSTOPB;
	else
		tty.c_cflag &= ~CSTOPB;

	ret = tcsetattr(fd, TCSANOW, &tty);

	setRTS(fd);
#endif /* POSIX_TERMIOS */

#ifndef _DCDFLOW
	setHardwareFlow(fd, hwf);
#endif

    if (ret != 0)
        return 0xFFFF000D;
    else 
        return 0;
}

/*
 * Wait for child and return pid + status
 int m_wait(stt)
 int *stt;
 {
#if defined (_BSD43) && !defined(POSIX_TERMIOS)
int pid;
union wait st1;

pid = wait((void *)&st1);
 *stt = (unsigned)st1.w_retcode + 256 * (unsigned)st1.w_termsig;
 return(pid);
#else
int pid;
int st1;

pid = wait(&st1);
 *stt = (unsigned)WEXITSTATUS(st1) + 256 * (unsigned)WTERMSIG(st1);
 return(pid);
#endif
}
*/
/*
 * Set cbreak mode.
 * Mode 0 = normal.
 * Mode 1 = cbreak, no echo
 * Mode 2 = raw, no echo.
 * Mode 3 = only return erasechar (for wkeys.c)
 *
 * Returns: the current erase character.
 */  
int setCBreakMode(int fd, int mode)
{
#ifdef POSIX_TERMIOS
	struct termios tty;
	static int init = 0;
	static int erasechar;
#ifdef _HPUX_SOURCE
	static int lastmode = -1;
#endif

#ifndef XCASE
#  ifdef _XCASE
#    define XCASE _XCASE
#  else
#    define XCASE 0
#  endif
#endif

	if (init == 0) {
		tcgetattr(fd, &savetty);
		erasechar = savetty.c_cc[VERASE];
		init++;
	}

	if (mode == 3) return(erasechar);

#ifdef _HPUX_SOURCE
	/* In HP/UX, TCSADRAIN does not work for me. So we use only RAW
	 * or NORMAL mode, so we never have to switch from cbreak <--> raw
	 */
	if (mode == 1) mode = 2;
#endif

	/* Avoid overhead */
#ifdef _HPUX_SOURCE
	if (mode == lastmode) return(erasechar);
	lastmode = mode;
#endif

	/* Always return to default settings first */
	tcsetattr(fd, TCSADRAIN, &savetty);

	if (mode == 0) {
		return(erasechar);
	}

	tcgetattr(fd, &tty);
	if (mode == 1) {
		tty.c_oflag &= ~OPOST;
		tty.c_lflag &= ~(XCASE|ECHONL|NOFLSH);
		tty.c_lflag &= ~(ICANON | ISIG | ECHO);
		tty.c_iflag &= ~(ICRNL|INLCR);
		tty.c_cflag |= CREAD;
		tty.c_cc[VTIME] = 5;
		tty.c_cc[VMIN] = 1;
	}
	if (mode == 2) { /* raw */
		tty.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC | 
				IXANY | IXON | IXOFF | INPCK | ISTRIP);
		tty.c_iflag |= (BRKINT | IGNPAR);
		tty.c_oflag &= ~OPOST;
		tty.c_lflag &= ~(XCASE|ECHONL|NOFLSH);
		tty.c_lflag &= ~(ICANON | ISIG | ECHO);
		tty.c_cflag |= CREAD;
		tty.c_cc[VTIME] = 5;
		tty.c_cc[VMIN] = 1;
	}	
	tcsetattr(fd, TCSADRAIN, &tty);
	return(erasechar);
#else
	struct sgttyb args;
	static int init = 0;
	static int erasechar;
#ifdef _BSD43  
	static struct ltchars ltchars;
#endif

	if (init == 0) {
		(void) ioctl(fd, TIOCGETP, &savetty);
		(void) ioctl(fd, TIOCGETC, &savetty2);
#ifdef _BSD43
		(void) ioctl(fd, TIOCGLTC, &ltchars);
#endif
		erasechar = savetty.sg_erase;
		init++;
	}

	if (mode == 3) return(erasechar);

	if (mode == 0) {
		(void) ioctl(fd, TIOCSETP, &savetty);
		(void) ioctl(fd, TIOCSETC, &savetty2);
#ifdef _BSD43
		(void) ioctl(fd, TIOCSLTC, &ltchars);
#endif
		return(erasechar);
	}

	(void) ioctl(fd, TIOCGETP, &args);
	if (mode == 1) {
		args.sg_flags |= CBREAK;
		args.sg_flags &= ~(ECHO|RAW);
	}
	if (mode == 2) {
		args.sg_flags |= RAW;
		args.sg_flags &= ~(ECHO|CBREAK);
	}
	(void) ioctl(fd, TIOCSETP, &args);
	return(erasechar);
#endif
}

int timeRead(int fd, char *buf, int size, int timeout) {
	int ret;
	fd_set readFDs;
	struct timeval tv;

	if(timeout > 0) {
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		FD_ZERO(&readFDs);
		FD_SET(fd, &readFDs);
		ret = select(fd+1, &readFDs, NULL, NULL, &tv);
		if(ret <= 0) {
			return ret;
		}
		if(FD_ISSET(fd, &readFDs)) {
			return read(fd, buf, size);
		}
		return 0;
	}
	return read(fd, buf, size);
}
