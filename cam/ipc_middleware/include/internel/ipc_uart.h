#ifndef __IPC_UART_H__
#define __IPC_UART_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

typedef struct ipc_uart_attr
{
    s32 speed;      /* UART baud rate: 115200, 38400, 19200, 9600, 4800, 2400, 1200, 300 */
    s32 databits;   /* data bits: 5 or 6 or 7 or 8 */
    s32 stopbits;   /* stop bits: 1 or 2 */
    v8 parity;      /* parity type: N or E or O or S */
}IPC_UART_ATTR_S, *P_IPC_UART_ATTR_S;

/**
 * @brief UART initialization
 *
 * @param[in] tty_dev: device node name, such as /det/ttyUSB0
 * @param[in] uart_attr: UART attributes IPC_UART_ATTR_S, if NULL, default attributes are baud rate:115200, data bits:8, stop bits:1, parity type:N
 * @return success: UART device file descriptor, failure: IPC_NOT_INIT
 */
s32 ipc_uart_init(pcv8 tty_dev, P_IPC_UART_ATTR_S uart_attr);

/**
 * @brief UART resource release
 *
 * @param[in] uart_fd: file descriptor returned by ipc_uart_init
 */
void ipc_uart_uninit(s32 uart_fd);

/**
 * @brief UART data reading
 *
 * @param[in] uart_fd: file descriptor returned by ipc_uart_init
 * @param[out] data: data read from UART
 * @param[in] data_size: size of data
 * @param[in] timeout_ms: timeout in ms
 * @return success: data size read, failure: CP error code
 */
s32 ipc_uart_read(s32 uart_fd, pv8 data, s32 data_size, s32 timeout_ms);

/**
 * @brief UART data writing
 *
 * @param[in] uart_fd: file descriptor returned by ipc_uart_init
 * @param[in] data: data to write to UART
 * @param[in] data_size: size of data to write to UART
 * @return success: data size written, failure: <0
 */
s32 ipc_uart_write(s32 uart_fd, pv8 data, s32 data_size);

#ifdef __cplusplus
}
#endif

#endif
