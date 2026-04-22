#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>    /*PPSIX terminal control definition*/
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ipc_core.h>
#include <ipc_log.h>
#include "internel/ipc_uart.h"

s32 ipc_uart_init(pcv8 tty_dev, P_IPC_UART_ATTR_S uart_attr)
{
    /* coverity[assigned_value] */
    s32 uart_fd = -1;
    s32 result = -1;
    s32 num = 0;
    s32 speed_arr[] = {B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300};
    s32 name_arr[]  = {115200,  38400,  19200,  9600,  4800,  2400,  1200,  300};
    struct termios options;

    clog_init("uart", "uart control");

    /* Default baud rate is 115200, data bits is 8, stop bits is 1, parity type is N*/
    IPC_UART_ATTR_S uart_attr_cur = {
        .speed    = 115200,
        .databits = 8,
        .stopbits = 1,
        .parity   = 'N'
    };

    if(NULL != uart_attr) {
        memcpy(&uart_attr_cur, uart_attr, sizeof(IPC_UART_ATTR_S));
    }
    ipcinfo("%s speed: %d, databis: %d, stopbits: %d, parity: %c\n",
        __func__, uart_attr_cur.speed, uart_attr_cur.databits, uart_attr_cur.stopbits, uart_attr_cur.parity);

    uart_fd = open(tty_dev, O_RDWR | O_NOCTTY);
    if(uart_fd < 0) {
        ipcerror("Can't Open TTY_DEV : %s", tty_dev);
        goto INIT_FAILED;
    }

    /* Check if it is a terminal device */
    result = isatty(uart_fd);
    if(0 == result) {
        ipcerror("Standard input is not a terminal device\n");
        goto INIT_FAILED;
    }

    /* Get related parameters of serial device, this function can test whether configuration is correct and judge whether serial device is available, return 0 on success, -1 on failure */
    result = tcgetattr(uart_fd, &options);
    if(0 != result) {
        ipcerror("get TTY_DEV: %s attr failed\n", tty_dev);
        goto INIT_FAILED;
    }

    // cfmakeraw(&options);
#if 1
    /* Modify control mode to ensure program does not occupy serial port */
    options.c_cflag |= CLOCAL;
    /* Modify control mode to enable reading input data from serial port */
    options.c_cflag |= CREAD;
    /* Set character size */
    options.c_cflag &= ~CSIZE;
    /* Sometimes, when using write to send data without pressing enter, the information cannot be sent,
       this is mainly because we send only when receiving carriage return or line feed in canonical mode,
       but in most cases we don't need to press enter or line feed,
       at this time should convert to line mode input, send directly without processing, set as follows: */
    options.c_lflag  &= ~(ICANON | ECHO | ECHOE | ISIG);
    /* Modify output mode to raw data output */
    options.c_oflag  &= ~OPOST;
    /* Sometimes, when sending character 0X0d, the receiving end often gets character 0X0a,
       the reason is that there is a mapping from NL-CR and CR-NL in c_iflag and c_oflag in serial port settings,
       that is, serial port can treat carriage return and line feed as the same character, can be blocked by the following settings: */
    options.c_iflag &= ~ (INLCR | ICRNL | IGNCR);
    options.c_oflag &= ~(ONLCR | OCRNL);
    // options.c_iflag &= ~(IXON);
#endif
    /* Set baud rate */
    for (num= 0; num < sizeof(speed_arr)/sizeof(int); num++)
    {
        if (uart_attr_cur.speed == name_arr[num]) {
            cfsetispeed(&options, speed_arr[num]);
            cfsetospeed(&options, speed_arr[num]);
            break;
        }
    }
    if(num == sizeof(speed_arr)/sizeof(int)) {
        ipcerror("Unsupported speed: %d\n", uart_attr_cur.speed);
        goto INIT_FAILED;
    }
    /* Set data bits */
    switch(uart_attr_cur.databits)
    {
        case 5:
            options.c_cflag |= CS5;
            break;
        case 6:
            options.c_cflag |= CS6;
            break;
        case 7:
            options.c_cflag |= CS7;
            break;
        case 8:
            options.c_cflag |= CS8;
            break;
        default:
            ipcerror("Unsupported data size: %d\n", uart_attr_cur.databits);
            goto INIT_FAILED;
    }
    /* Set parity type */
    switch(uart_attr_cur.parity)
	{
		case 'n':
		case 'N':   /* No parity bit */
			options.c_cflag &= ~PARENB;    /* Clear parity enable */
			options.c_iflag &= ~INPCK;	   /* Enable parity checking */
			break;
		case 'o':
		case 'O':   /* Odd parity */
			options.c_cflag |= (PARODD | PARENB); /* Set to odd parity*/
			options.c_iflag |= INPCK;			  /* Disable parity checking */
			break;
		case 'e':
		case 'E':   /* Even parity */
			options.c_cflag |= PARENB;	    /* Enable parity */
			options.c_cflag &= ~PARODD;     /* Convert to even parity*/
			options.c_iflag |= INPCK;		/* Disable parity checking */
			break;
		case 'S':
		case 's':   /* Set to space */
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
			break;
		default:
			ipcerror("Unsupported parity: %c\n", uart_attr_cur.parity);
			goto INIT_FAILED;
	}
    /* Set stop bits */
	switch(uart_attr_cur.stopbits)
	{
		case 1:
			options.c_cflag &= ~CSTOPB;
			break;
		case 2:
		    options.c_cflag |= CSTOPB;
		    break;
		default:
		    ipcerror("Unsupported stop bits: %d\n", uart_attr_cur.stopbits);
		    goto INIT_FAILED;
	}

    /* Set wait time and minimum receive characters */
    options.c_cc[VTIME] = 0; /* wait time*/
	options.c_cc[VMIN] = 0;  /* minimum receive characters */

    /* Clear all ongoing IO data */
    tcflush(uart_fd, TCIOFLUSH);

    /* TCSANOW: execute immediately without waiting for data transmission or reception to complete */
    result = tcsetattr(uart_fd, TCSANOW, &options);
    if(0 != result)
	{
		ipcerror("get TTY_DEV: %s attr failed\n", tty_dev);
		goto INIT_FAILED;
	}

    return uart_fd;;

INIT_FAILED:
    if(uart_fd >= 0) {
        close(uart_fd);
    }

    return IPC_NOT_INIT;
}

void ipc_uart_uninit(s32 uart_fd)
{
    if(uart_fd >= 0) {
        close(uart_fd);
    }
}

s32 ipc_uart_read(s32 uart_fd, pv8 data, s32 data_size, s32 timeout_ms)
{
    if(uart_fd < 0 || NULL == data) {
        ipcerror("%s arg error\n", __func__);
        return IPC_INVALID_ARGS;
    }

    s32 result = 0;
    fd_set fd_s;

    s32 readout_size = 0;
    s32 timeout_cnt = 0;
    s32 readed_timeout_cnt = 0;
    struct timeval tvSelect = {0};

    do{
        FD_ZERO(&fd_s);
        FD_SET(uart_fd, &fd_s);
        tvSelect.tv_sec = 0;
        tvSelect.tv_usec = 100000;

        result = select(uart_fd + 1, &fd_s, NULL, NULL, &tvSelect);
        ipcdebug("uart fd select result[%d], timeout_cnt[%d]\n", result, timeout_cnt);
        // if (FD_ISSET(uart_fd, &fd_s) > 0) {
        if(result > 0) {
            result = read(uart_fd, &data[readout_size], data_size);
            if(result < 0) {
                break;
            }

            readed_timeout_cnt = 1;

            readout_size += result;
            data_size -= result;
            if (data_size <= 0) {
                return readout_size;
            }
        }

        timeout_cnt+=100;
        if(timeout_cnt > timeout_ms) {
            result = readout_size;
            break;
        }

        if (readed_timeout_cnt > 0) {
            readed_timeout_cnt += 100;
            if (readed_timeout_cnt > 3000) {
                result = readout_size;
                break;
            }
        }
    }while(1);

    return result;
}

s32 ipc_uart_write(s32 uart_fd, pv8 data, s32 data_size)
{
    if(uart_fd < 0 || NULL == data) {
        ipcerror("%s arg error\n", __func__);
        return IPC_INVALID_ARGS;
    }

    /* coverity[assigned_value] */
    s32 result = -1;

    result = write(uart_fd, data, data_size);

	return result;
}
