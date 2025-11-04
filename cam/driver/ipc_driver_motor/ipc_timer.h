#ifndef __IPC_TIMER_H__
#define __IPC_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ipc_timer_handler_t)(int timer_index, void* arg);

int ipc_timer_init(int timer_num);                       // Timer initialization
int ipc_timer_set_period(int timer_index, int time10us); // Set timeout time period, minimum unit of 0.01ms
int ipc_timer_start(int timer_index);
int ipc_timer_stop(int timer_index);
int ipc_timer_set_timeout_cb(ipc_timer_handler_t cb, void* arg);
int ipc_timer_uninit(void);

#ifdef __cplusplus
}
#endif

#endif //__TIMER_H__
