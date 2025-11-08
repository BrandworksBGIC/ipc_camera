#include "ipc_aac.h"
#include "ipc_std.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vo-aacenc/voAAC.h>
#include <vo-aacenc/cmnMemory.h>
#include <pthread.h> // Multi-threading support

#define PCM_CACHE_LEN 4096

// Encoder context structure (contains all state variables)
typedef struct {
    VO_HANDLE               vo_handle;                // VO-AAC encoder handle
    VO_AUDIO_CODECAPI       codec_api;                // VO-AAC API functions
    VO_MEM_OPERATOR         mem_operator;             // Memory operator functions
    s32                     sample_rate;              // Sample rate
    s32                     channels;                 // Number of channels
    s32                     bitrate;                  // Bit rate
    s32                     g_BlockLength;            // Block length
    short*                  g_pSrcBuf;                // Source PCM buffer
    unsigned char*          g_pDstBuf;                // Destination AAC buffer
    v8                      pcm_cache[PCM_CACHE_LEN]; // PCM cache
    s32                     pcm_cache_len;            // Cache length
    pthread_mutex_t         mutex;                    // Mutex to protect state access
} IPRTS_AAC_ENCODE_CONTEXT;

// ----------------------------------------------------------------------------
// AAC encoder initialization, returns thread-safe encoder context
// ----------------------------------------------------------------------------
vptr ipc_aac_encode_open(s32 bit_width, s32 sample_rate, s32 channel)
{
    // Allocate encoder context structure
    IPRTS_AAC_ENCODE_CONTEXT* ctx = (IPRTS_AAC_ENCODE_CONTEXT*)malloc(sizeof(IPRTS_AAC_ENCODE_CONTEXT));
    if (!ctx) {
        fprintf(stderr, "malloc encoder context failed\n");
        return NULL;
    }
    memset(ctx, 0, sizeof(IPRTS_AAC_ENCODE_CONTEXT));

    // Initialize mutex
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        free(ctx);
        return NULL;
    }

    // Store parameters
    ctx->sample_rate = sample_rate;
    ctx->channels = channel;
    ctx->bitrate = 8000; // Default bitrate

    // Initialize VO-AAC encoder API
    voGetAACEncAPI(&ctx->codec_api);

    // Setup memory operator
    ctx->mem_operator.Alloc = cmnMemAlloc;
    ctx->mem_operator.Copy = cmnMemCopy;
    ctx->mem_operator.Free = cmnMemFree;
    ctx->mem_operator.Set = cmnMemSet;
    ctx->mem_operator.Check = cmnMemCheck;

    // Initialize encoder
    VO_CODEC_INIT_USERDATA user_data;
    user_data.memflag = VO_IMF_USERMEMOPERATOR;
    user_data.memData = &ctx->mem_operator;

    if (ctx->codec_api.Init(&ctx->vo_handle, VO_AUDIO_CodingAAC, &user_data) != VO_ERR_NONE) {
        fprintf(stderr, "VO-AAC encoder init failed\n");
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return NULL;
    }

    // Set encoding parameters
    AACENC_PARAM params = { 0 };
    params.sampleRate = sample_rate;
    params.bitRate = ctx->bitrate;
    params.nChannels = channel;
    params.adtsUsed = 1; // Use ADTS format

    if (ctx->codec_api.SetParam(ctx->vo_handle, VO_PID_AAC_ENCPARAM, &params) != VO_ERR_NONE) {
        fprintf(stderr, "VO-AAC set parameters failed\n");
        ctx->codec_api.Uninit(ctx->vo_handle);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return NULL;
    }

    // Calculate block length (1024 samples per frame for AAC)
    ctx->g_BlockLength = channel * sizeof(short) * 1024;
    printf("AAC encoder initialized with sample rate: %d, bit rate: %d, channel: %d\n",
           sample_rate, ctx->bitrate, channel);
    printf("Single frame byte length: %d\n", ctx->g_BlockLength);

    // Allocate codec buffers
    ctx->g_pSrcBuf = (short*)malloc(ctx->g_BlockLength);
    ctx->g_pDstBuf = (unsigned char*)malloc(ctx->g_BlockLength);
    if (!ctx->g_pSrcBuf || !ctx->g_pDstBuf) {
        fprintf(stderr, "malloc buffers failed\n");
        ctx->codec_api.Uninit(ctx->vo_handle);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx->g_pSrcBuf);
        free(ctx->g_pDstBuf);
        free(ctx);
        return NULL;
    }

    return (vptr)ctx;
}

// ----------------------------------------------------------------------------
// AAC encoder deinitialization, releases resources (thread-safe)
// ----------------------------------------------------------------------------
void ipc_aac_encode_close(vptr handle)
{
    if (!handle)
        return;

    IPRTS_AAC_ENCODE_CONTEXT* ctx = (IPRTS_AAC_ENCODE_CONTEXT*)handle;

    // Lock to protect resource release
    pthread_mutex_lock(&ctx->mutex);

    if (ctx->vo_handle) {
        ctx->codec_api.Uninit(ctx->vo_handle);
        ctx->vo_handle = NULL;
    }
    free(ctx->g_pSrcBuf);
    free(ctx->g_pDstBuf);
    ctx->g_pSrcBuf     = NULL;
    ctx->g_pDstBuf     = NULL;
    ctx->pcm_cache_len = 0;

    pthread_mutex_unlock(&ctx->mutex);
    pthread_mutex_destroy(&ctx->mutex);
    free(ctx);
}

// ----------------------------------------------------------------------------
// AAC encoding iterator (thread-safe version)
// ----------------------------------------------------------------------------
ipc_aac_data_p ipc_aac_encode_iter(vptr handle, vptr pcm_data, s32 pcm_len)
{
    static ipc_aac_data_t out = { .buf = NULL, .len = 0 };
    out.buf                  = NULL;
    out.len                  = 0;

    if (!handle || !pcm_data || pcm_len <= 0)
        return NULL;

    IPRTS_AAC_ENCODE_CONTEXT* ctx = (IPRTS_AAC_ENCODE_CONTEXT*)handle;

    // Lock to protect encoder state access
    pthread_mutex_lock(&ctx->mutex);

    if (!ctx->vo_handle || ctx->g_BlockLength <= 0) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }

    s32 pcm_idx = 0;

    while (1) {
        s32 left_len  = pcm_len - pcm_idx;
        unsigned char* left_data = (unsigned char*)pcm_data + pcm_idx;

        // Check if cache has enough data to assemble one frame
        if (ctx->pcm_cache_len + left_len < ctx->g_BlockLength)
            break;

        // Fill cache and encode
        s32 copy_len = ctx->g_BlockLength - ctx->pcm_cache_len;
        memcpy(ctx->pcm_cache + ctx->pcm_cache_len, left_data, copy_len);
        ctx->pcm_cache_len = 0;
        pcm_idx += copy_len;

        // Copy frame to source buffer
        memcpy(ctx->g_pSrcBuf, ctx->pcm_cache, ctx->g_BlockLength);

        // Set input data for VO-AAC encoder
        VO_CODECBUFFER input = { 0 };
        input.Buffer = (unsigned char*)ctx->g_pSrcBuf;
        input.Length = ctx->g_BlockLength;

        if (ctx->codec_api.SetInputData(ctx->vo_handle, &input) != VO_ERR_NONE) {
            fprintf(stderr, "VO-AAC SetInputData failed\n");
            pthread_mutex_unlock(&ctx->mutex);
            return NULL;
        }

        // Get encoded output
        VO_CODECBUFFER output = { 0 };
        VO_AUDIO_OUTPUTINFO output_info = { 0 };
        output.Buffer = ctx->g_pDstBuf;
        output.Length = ctx->g_BlockLength; // Maximum output size

        if (ctx->codec_api.GetOutputData(ctx->vo_handle, &output, &output_info) == VO_ERR_NONE) {
            out.buf = ctx->g_pDstBuf;
            out.len = output.Length;
            pthread_mutex_unlock(&ctx->mutex);
            return &out;
        } else {
            fprintf(stderr, "VO-AAC GetOutputData failed\n");
            pthread_mutex_unlock(&ctx->mutex);
            return NULL;
        }
    }

    // Handle remaining PCM data
    s32 remaining = pcm_len - pcm_idx;
    if (remaining > 0) {
        if (ctx->pcm_cache_len + remaining > sizeof(ctx->pcm_cache)) {
            fprintf(stderr, "pcm_cache overflow, clearing cache\n");
            ctx->pcm_cache_len = 0;
            pthread_mutex_unlock(&ctx->mutex);
            return NULL;
        }
        memcpy(ctx->pcm_cache + ctx->pcm_cache_len, (unsigned char*)pcm_data + pcm_idx, remaining);
        ctx->pcm_cache_len += remaining;
    }

    pthread_mutex_unlock(&ctx->mutex);
    return NULL;
}

vptr ipc_aac_decode_open(void)
{
    return NULL;
}

void ipc_aac_decode_close(vptr handle)
{
}

ipc_aac_data_p ipc_aac_decode_iter(vptr handle, vptr aac_data, s32 aac_len, ipc_aac_info_p info)
{

    return NULL;
}
