#ifndef __IPC_VOICE_FILE_H__
#define __IPC_VOICE_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "wave.h"
#include <ipc_core.h>

typedef enum {
    IPC_VOICE_WAV,
    IPC_VOICE_PCM,
    IPC_VOICE_G711U,
    IPC_VOICE_G711A,
} ipc_voice_type_e;

typedef struct {
    ipc_voice_type_e file_type; ///< File type (ipc_voice_type_e)
    s32 codec_type;            ///< Codec type
    union {                    ///< File handle
        ipc_file_t h_file;
        WaveFile* h_fp;
    };
} voice_t, *voice_p;

s32 ipc_voice_open(voice_p h_voice, pv8 file, ipc_voice_type_e type);
void ipc_voice_close(voice_p h_voice);
void ipc_voice_reset(voice_p h_voice);
s32 ipc_voice_read(voice_p h_voice, pv8 buf, s32 max);

#ifdef __cplusplus
}
#endif

#endif //__IPC_VOICE_FILE_H__