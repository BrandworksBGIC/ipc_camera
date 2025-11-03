/*
 * Copyright (c) 2022 Realtek Semiconductor Corp. All rights reserved.
 *
 * SPDX-License-Identifier: LicenseRef-Realtek-Proprietary
 *
 * This software component is confidential and proprietary to Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in whole
 * or in part, of this work and its derivatives without express permission
 * is prohibited.
 */

 /**
  * @file rtsaudio.h
  * @brief RTStream audio API
  * @defgroup audio RTStream Audio API
  * @brief RTStream audio API reference
  * @details
  * \htmlonly <style>div.image img[src="audio_flow.png"]{width:600px;}</style> \endhtmlonly
  * \image html audio_flow.png "RTStream Audio Flow Diagram"
  * @{
  */

#ifndef _INCLUDE_RTSAUDIO_H
#define _INCLUDE_RTSAUDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @example example_audio_server.c
 * @example example_audio_decode_playback.c
 * @example example_audio_playback.c
 * @example example_audio_capture.c
 * @example example_audio_capture_encode.c
 * @example example_audio_encode.c
 * @example example_audio_decode.c
 * @example example_audio_resample.c
 */

/**
 * @brief Create audio playback channel
 * @param[in] attr pointer to rts_audio_attr
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_playback_chn(struct rts_audio_attr *attr);

/**
 * @brief Get the attribute of audio playback channel
 * @param[in] chnno playback channel number
 * @param[out] attr pointer to rts_audio_attr
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_playback_attr(unsigned int chnno,
				struct rts_audio_attr *attr);

/**
 * @brief Create audio capture channel
 * @param[in] attr pointer to rts_audio_attr
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_capture_chn(struct rts_audio_attr *attr);

/**
 * @brief Get the attribute of audio capture channel
 * @param[in] chnno capture channel number
 * @param[out] attr pointer to rts_audio_attr
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_capture_attr(unsigned int chnno,
				struct rts_audio_attr *attr);

/**
 * @brief Create audio encode channel
 * @param[in] codec_id encode type
 * @param[in] bitrate bit rate
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_encode_chn(int codec_id, uint32_t bitrate);

/**
 * @brief Get the attribute of audio encode channel
 * @param[in] chnno capture channel number
 * @param[out] codec_id encode type
 * @param[out] bitrate bit rate
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_encode_attr(unsigned int chnno,
				int *codec_id, uint32_t *bitrate);

/**
 * @brief Set the attribute of audio encode channel
 * @param[in] chnno capture channel number
 * @param[in] codec_id encode type
 * @param[in] bitrate bit rate
 * @return 0 : success, minus : fail
 */
int rts_av_set_audio_encode_attr(unsigned int chnno,
				int codec_id, uint32_t bitrate);

/**
 * @brief Create audio decode channel
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_decode_chn(void);

/**
 * @brief Create audio resample channel
 * @param[in] rate sample rate
 * @param[in] format bit width
 * @param[in] channels sound track
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_resample_chn(uint32_t rate, uint32_t format,
				uint32_t channels);

/**
 * @brief Get the attribute of audio resample channel
 * @param[in] chnno resample channel number
 * @param[out] rate sample rate
 * @param[out] format bit width
 * @param[out] channels sound track
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_resample_attr(unsigned int chnno, uint32_t *rate,
				uint32_t *format, uint32_t *channels);

/**
 * @brief Set the attribute of audio resample channel
 * @param[in] chnno resample channel number
 * @param[in] rate sample rate
 * @param[in] format bit width
 * @param[in] channels sound track
 * @return 0 : success, minus : fail
 */
int rts_av_set_audio_resample_attr(unsigned int chnno, uint32_t rate,
				uint32_t format, uint32_t channels);

/**
 * @brief Create audio mixer channel
 * @return 0 : success, minus : fail
 */
int rts_av_create_audio_mixer_chn(void);

/**
 * @brief Set whether the audio data can be dropped
 * @param[in] mixer mixer channel number
 * @param[in] src source for mixer channel
 * @param[in] droppable 1 : droppable, 0 : no droppable
 * @return 0 : success, minus : fail
 */
int rts_av_set_audio_mixer_droppable(unsigned int mixer, unsigned int src,
				int droppable);

/**
 * @brief audio encode type
 */
enum RTS_AUDIO_TYPE_ID {
	RTS_AUDIO_TYPE_ID_ULAW = 2, /**< ULAW */
	RTS_AUDIO_TYPE_ID_ALAW, /**< ALAW */
	RTS_AUDIO_TYPE_ID_PCM, /**< PCM */
	RTS_AUDIO_TYPE_ID_G726, /**< G.726 */
};

/**
 * @brief denoise and AEC attribute description
 */
struct rts_audio_aecns_attr {
	uint8_t aec_enable; /**< enable AEC */
	uint8_t ns_enable; /**< enable denoise */
	uint8_t dump_enable; /**< enable to dump pcm data */
	uint32_t ns_level;
	uint32_t aec_scale; /**< coefficient of amplification for AEC */
	char mic_name[256]; /**< file path used to save mic data */
	char spk_name[256]; /**< file path used to save speaker data */
	char aec_name[256]; /**< file path used to save aec out data */
	int32_t aec_thr; /**< threshold (dB) for mute, range from -100 to -1, 0 means always unmute */
	int32_t min_delay; /**< minimum delay in millisecond between mic and speaker */
	int32_t max_delay; /**< maximum delay in millisecond between mic and speaker */
};

/**
 * @brief EQ attribute description
 */
struct rts_audio_eq_attr {
	int eq_enable; /**< enable EQ */
	int dump_enable; /**< enable to dump pcm data */

	int gain_fre_31; /**< maximum gain (dB) in 31.25 Hz band */
	int gain_fre_63; /**< maximum gain (dB) in 62.5 Hz band */
	int gain_fre_125; /**< maximum gain (dB) in 125 Hz band */
	int gain_fre_250; /**< maximum gain (dB) in 250 Hz band */
	int gain_fre_500; /**< maximum gain (dB) in 500 Hz band */
	int gain_fre_1k; /**< maximum gain (dB) in 1k Hz band */
	int gain_fre_2k; /**< maximum gain (dB) in 2k Hz band */
	int gain_fre_4k; /**< maximum gain (dB) in 4k Hz band */
	int gain_fre_8k; /**< maximum gain (dB) in 8k Hz band */
	int gain_fre_16k; /**< maximum gain (dB) in 16k Hz band */
	char ori_name[256]; /**< file path used to save origin data */
	char eq_name[256]; /**< file path used to save eq out data */
};

/**
 * @brief AGC attribute description
 */
struct rts_audio_agc_attr {
	int agc_enable; /**< enable AGC */

	int target_level; /**< target output volume level (dBOv) */
	int compression_gain; /**< maximum lifting capacity (dB) */
	int limiter_enable; /**< enable limiter */
};

/**
 * @brief audio capture VQE attribute description
 */
struct rts_audio_capture_vqe {
	uint8_t aecns_enable; /**< enable aecns, aec_enable and ns_enable can be set only when aecns_enable is 1 */
	uint8_t eq_enable; /**< enable eq, eq_attr can be set successfully only when eq_enable is 1 */

	struct rts_audio_aecns_attr aecns_attr; /**<  aecns attr */
	struct rts_audio_eq_attr eq_attr; /**<  eq attr */
};

/**
 * @brief audio playback VQE attribute description
 */
struct rts_audio_playback_vqe {
	uint8_t eq_enable; /**< enable eq, eq_attr can be set successfully only when eq_enable is 1 */
	uint8_t agc_enable; /**< enable agc, agc_attr can be set successfully only when agc_enable is 1 */

	struct rts_audio_eq_attr eq_attr; /**< eq attr */
	struct rts_audio_agc_attr agc_attr; /**< agc attr */
};

/**
 * @brief audio playback control attribute description
 */
struct rts_playback_control {
	uint32_t cache_samples; /**< cached samples of playback ring buffer */
	int32_t status; /**< status of getting cached samples */
	uint32_t reserved[4];
};

/**
 * @brief audio mixer control attribute description
 */
struct rts_mixer_control {
	uint32_t mixer_thr; /**< threshold of mixer channel output buffer, effective when mixer_thr > 0 */
	uint32_t reserved[4];
};

/**
 * @brief Get audio capture VQE attribute
 * @param[in] chnno capture channel number
 * @param[out] c_vqe capture VQE attribute
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_capture_vqe(unsigned int chnno,
				struct rts_audio_capture_vqe *c_vqe);
/**
 * @brief Set audio capture VQE attribute
 * @param[in] chnno capture channel number
 * @param[in] c_vqe capture VQE attribute
 * @return 0 : success, minus : fail
 */
int rts_av_set_audio_capture_vqe(unsigned int chnno,
				struct rts_audio_capture_vqe *c_vqe);
/**
 * @brief Get audio playback VQE attribute
 * @param[in] chnno playback channel number
 * @param[out] p_vqe playback VQE attribute
 * @return 0 : success, minus : fail
 */
int rts_av_get_audio_playback_vqe(unsigned int chnno,
				struct rts_audio_playback_vqe *p_vqe);
/**
 * @brief Set audio playback VQE attribute
 * @param[in] chnno playback channel number
 * @param[in] p_vqe playback VQE attribute
 * @return 0 : success, minus : fail
 */
int rts_av_set_audio_playback_vqe(unsigned int chnno,
				struct rts_audio_playback_vqe *p_vqe);

/**
 * @brief Query audio playback control
 * @param[in] chnno playback channel number
 * @param[out] ppctrl playback control
 * @return 0 : success, minus : fail
 */
int rts_av_query_playback_ctrl(unsigned int chnno,
		struct rts_playback_control **ppctrl);
/**
 * @brief Release audio playback control
 * @param[in] ctrl playback control
 */
void rts_av_release_playback_ctrl(struct rts_playback_control *ctrl);
/**
 * @brief Get audio playback control attribute
 * @param[out] ctrl playback control
 * @return 0 : success, minus : fail
 */
int rts_av_get_playback_ctrl(struct rts_playback_control *ctrl);

/**
 * @brief Query audio mixer control
 * @param[in] chnno mixer channel number
 * @param[out] ppctrl mixer control
 * @return 0 : success, minus : fail
 */
int rts_av_query_amixer_ctrl(unsigned int chnno,
				struct rts_mixer_control **ppctrl);
/**
 * @brief Release audio mixer control
 * @param[in] ctrl mixer control
 */
void rts_av_release_amixer_ctrl(struct rts_mixer_control *ctrl);
/**
 * @brief Set audio mixer control attribute
 * @param[in] ctrl mixer control
 * @return 0 : success, minus : fail
 */
int rts_av_set_amixer_ctrl(struct rts_mixer_control *ctrl);

/**
 * @brief Check whether the audio encoding ID is valid
 * @param[in] id audio encoding ID, see \ref RTS_AUDIO_TYPE_ID
 * @return 0 : success, minus : fail
 */
int rts_audio_codec_check_encode_id(int id);

/**
 * @brief Check whether the audio decoding ID is valid
 * @param[in] id audio decoding ID, see \ref RTS_AUDIO_TYPE_ID
 * @return 0 : success, minus : fail
 */
int rts_audio_codec_check_decode_id(int id);

/**
 * @brief Initialize and obtain the ability of audio encoder
 * @param[in] chnno encode channel number
 * @param[out] ability pointer to the address of rts_audio_ability_t
 * @return 0 : success, minus : fail
 */
int rts_av_query_audio_encode_ability(unsigned int chnno,
				struct rts_audio_ability_t **ability);

/**
 * @brief Release the ability of audio encoder
 * @param[in] ability pointer to rts_audio_ability_t
 */
void rts_av_release_audio_encode_ability(struct rts_audio_ability_t *ability);
/*!@}*/

#ifdef __cplusplus
}
#endif
#endif
