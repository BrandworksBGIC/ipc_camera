/*
 * Realtek Semiconductor Corp.
 *
 * libs/include/rtsamixer.h
 *
 * Copyright (C) 2014      Wind Han<wind_han@realsil.com.cn>
 */
#ifndef _LIBS_INCLUDE_RTSAMIXER_H
#define _LIBS_INCLUDE_RTSAMIXER_H

/**
 * @file rtsamixer.h
 * @brief Audio volume control API
 * @defgroup vol Audio Volume Control
 * @brief API and Structure for Audio Volume Control
 * @ingroup audio
 * @{
 */

/**
 * @example example_audio_ctrl.c
 */

/**
 * @brief Get the playback volume
 * @param[out] per_vol pointer to playback volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_get_playback_volume(int *per_vol);

/**
 * @brief Set the playback volume
 * @param[in] per_vol playback volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_set_playback_volume(int per_vol);

/**
 * @brief Get the capture volume
 * @param[out] per_vol pointer to capture volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_get_capture_volume(int *per_vol);

/**
 * @brief Set the capture volume
 * @param[in] per_vol capture volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_set_capture_volume(int per_vol);

/**
 * @brief Get the capture volume
 * @param[out] per_l_vol pointer to capture left channel volume, 0 ~ 100
 * @param[out] per_r_vol pointer to capture right channel volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_get_capture_lr_volume(int *per_l_vol, int *per_r_vol);

/**
 * @brief Set the capture volume
 * @param[in] per_l_vol capture left channel volume, 0 ~ 100
 * @param[in] per_r_vol capture right channel volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_set_capture_lr_volume(int per_l_vol, int per_r_vol);

/**
 * @brief Get the capture analog volume
 * @param[out] per_vol pointer to capture analog volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_get_capture_analog_volume(int *per_vol);

/**
 * @brief Set the capture analog volume
 * @param[in] per_vol capture analog volume, 0 ~ 100
 * @return 0 : success, minus : fail
 */
int rts_audio_set_capture_analog_volume(int per_vol);

/**
 * @brief Playback mute
 * @return 0 : success, minus : fail
 */
int rts_audio_playback_mute(void);

/**
 * @brief Playback unmute
 * @return 0 : success, minus : fail
 */
int rts_audio_playback_unmute(void);

/**
 * @brief Capture mute
 * @return 0 : success, minus : fail
 */
int rts_audio_capture_mute(void);

/**
 * @brief Capture unmute
 * @return 0 : success, minus : fail
 */
int rts_audio_capture_unmute(void);

/**
 * @brief Set which channel is used when mono capture
 * @param[in] ch_val selected channel, 0 : left, 1 : right
 * @return 0 : success, minus : fail
 */
int rts_audio_capture_set_mono_in(int ch_val);
/*!@}*/
#endif
