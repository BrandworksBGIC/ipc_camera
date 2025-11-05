#ifndef __IPCRT_ALARM_H__
#define __IPCRT_ALARM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define IPRT_ODTINY_PERSON_RGB_THRESH 0.35
#define IPRT_ODTINY_PERSON_IR_THRESH 0.4

int ivrt_set_odtiny_parameter(const char* key, int size, void* data);

int ivrt_get_odtiny_parameter(const char *key, int size, void *data);

#ifdef __cplusplus
}
#endif

#endif