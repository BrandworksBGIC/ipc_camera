#include "ipc_voice_file.h"

s32 ipc_voice_open(voice_p h_voice, pv8 file, ipc_voice_type_e type)
{
    s32 ret = 0;
    h_voice->file_type = type;

    switch (type) {
        case IPC_VOICE_WAV:
            wave_err_clear();
            h_voice->h_fp = wave_open(file, WAVE_OPEN_READ);
            if (h_voice->h_fp == NULL) return IPC_NOMEM;
            WAVE_CONST WaveErr* err = wave_err();
            if (!err || err->code != WAVE_OK) {
                wave_close(h_voice->h_fp);
                return IPC_OPEN_ERROR;
            }
            h_voice->codec_type = wave_get_format(h_voice->h_fp);
        break;
        default: 
            ret = ipc_file_open(h_voice->h_file, file, IPC_FILE_RDONLY, __IPC_LOG__);
            if (ret < 0) return ret;
        break;
    }
    return IPC_SUCCESS;
}

void ipc_voice_close(voice_p h_voice)
{
    switch (h_voice->file_type) {
        case IPC_VOICE_WAV:
            wave_close(h_voice->h_fp);
        break;
        default:
            ipc_file_close(h_voice->h_file);
        break;
    }
}

void ipc_voice_reset(voice_p h_voice)
{
    switch (h_voice->file_type) {
        case IPC_VOICE_WAV:
            wave_seek(h_voice->h_fp, 0, SEEK_SET);
        break;
        default:
            ipc_file_seek(h_voice->h_file, 0, IPC_SEEK_HEAD);
        break;
    }
}

s32 ipc_voice_read(voice_p h_voice, pv8 buf, s32 max)
{
    s32 len = 0;
    s32 encode_max = max / 2;
    v8 encode_buff[encode_max];

    switch (h_voice->file_type) {
        case IPC_VOICE_WAV:
            if (h_voice->codec_type == WAVE_FORMAT_ALAW) {
                len = wave_read(h_voice->h_fp, encode_buff, encode_max);
                if (len < 0) return IPC_READ_ERROR;
                return len == 0 ? 0 : ipc_g711a_decode(buf, encode_buff, len);
            } else if (h_voice->codec_type == WAVE_FORMAT_MULAW) {
                len = wave_read(h_voice->h_fp, encode_buff, encode_max);
                if (len < 0) return IPC_READ_ERROR;
                return len == 0 ? 0 : ipc_g711u_decode(buf, encode_buff, len);
            } else {
                s32 block_size = wave_get_valid_bits_per_sample(h_voice->h_fp) / 8;
                len = wave_read(h_voice->h_fp, buf, max / block_size);
                return len < 0 ? IPC_READ_ERROR : len * block_size;
        }
        break;
        case IPC_VOICE_PCM: 
            return ipc_file_read(h_voice->h_file, buf, max);
        break;
        case IPC_VOICE_G711U:
            len = ipc_file_read(h_voice->h_file, encode_buff, encode_max);
            return len == 0 ? 0 : ipc_g711u_decode(buf, encode_buff, len);
        case IPC_VOICE_G711A:
            len = ipc_file_read(h_voice->h_file, encode_buff, encode_max);
            return len == 0 ? 0 : ipc_g711a_decode(buf, encode_buff, len);
        default: 
        break;
    }

    return IPC_NOT_SUPPORT;
}