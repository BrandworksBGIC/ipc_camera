/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <BasicUsageEnvironment.hh>
#include <H264VideoRTPSink.hh>
#include <H265VideoRTPSink.hh>
#include <SimpleRTPSink.hh>
#include <AMRAudioRTPSink.hh>
#include <MPEG4GenericRTPSink.hh>
#include <liveMedia.hh>
#include <rtsavapi.h>
#include <rtsvideo.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "RTSRtspd.hh"
#include "json_func.c"

#define H265_NALU_STARTCODE_LEN 4
#define H264_NALU_STARTCODE_LEN 4

#define H26X_INFO_SHARE_FILE  "/var/tmp/h25x_parse_info.shm"
#define ENCODE_TYPE_H264      0
#define ENCODE_TYPE_H265      1

typedef struct {
	unsigned char vps[64];
	unsigned char sps[64];
	unsigned char pps[64];
	int vps_len;
	int sps_len;
	int pps_len;
} h26x_parse_info;

///////////////////////  RingbufferVideoSource H264  //////////////////////////////

RBVideoH264Source *RBVideoH264Source::createNew(
	UsageEnvironment &env,
	char const *filename)
{
	return new RBVideoH264Source(env, filename);
}

RBVideoH264Source::RBVideoH264Source(UsageEnvironment &env,
		const char *filename)
: FramedSource(env), fFilename(NULL), fContext(NULL), fFirstTimeStamp(0), fLastPlayTime(0)
{
	fFilename = strDup(filename);

	fContext = rts_ringbuffer_init(fFilename, 0);
	if (fContext) {
		rts_ringbuffer_get_stream_format(fContext, &fFmt);
		fState = True;
	}
	wait_idr = 1;
}

RBVideoH264Source::~RBVideoH264Source(void)
{
	fState = False;
	if (fFilename)
		delete fFilename;

	if (fContext) {
		rts_ringbuffer_release(fContext);
		fContext = NULL;
	}
}

void RBVideoH264Source:: doGetNextFrame(void)
{
	int ret = 0;
	unsigned len;
	packet_t *pkt;

	fFrameSize = 0;
	ret = rts_ringbuffer_read_packet(fContext, &pkt);
	if (ret == 0) {
		if (wait_idr) {
		       if (pkt->flags & RTSTREAM_PKT_FLAG_KEY)
			       wait_idr = 0;
		       else
			       goto out;
		}

		rts_ringbuffer_get_stream_format(fContext, &fFmt);

		len = pkt->length - H264_NALU_STARTCODE_LEN;
		if (len > fMaxSize)
		{
			fNumTruncatedBytes = len - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else {
			fFrameSize = len;
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, (char *)pkt->vm_addr + H264_NALU_STARTCODE_LEN, fFrameSize);
		gettimeofday(&fPresentationTime, NULL);
		if (fFirstTimeStamp == 0) {
			fFirstTimeStamp = 1;
			fLastPlayTime = pkt->timestamp;
			fDurationInMicroseconds = 1000000 / fFmt.video.denominator;
		} else {
			unsigned uSeconds = fPresentationTime.tv_usec + pkt->timestamp - fLastPlayTime;
			fDurationInMicroseconds = pkt->timestamp - fLastPlayTime;
			fLastPlayTime = pkt->timestamp;
		}
out:
		rts_ringbuffer_free_packet(pkt);
	}
	envir().taskScheduler().scheduleDelayedTask(
		4000,
		(TaskFunc*)FramedSource::afterGetting,
		this);
}

///////////////////////  RingbufferVideoSource H265  //////////////////////////////
RBVideoH265Source *RBVideoH265Source::createNew(
	UsageEnvironment &env,
	char const *filename)
{
	return new RBVideoH265Source(env, filename);
}

RBVideoH265Source::RBVideoH265Source(UsageEnvironment &env,
		const char *filename)
: FramedSource(env), fFilename(NULL), fContext(NULL), fFirstTimeStamp(0), fLastPlayTime(0)
{
	fFilename = strDup(filename);

	fContext = rts_ringbuffer_init(fFilename, 0);
	if (fContext) {
		rts_ringbuffer_get_stream_format(fContext, &fFmt);
		fState = True;
	}
	wait_idr = 1;
}

RBVideoH265Source::~RBVideoH265Source(void)
{
	fState = False;
	if (fFilename)
		delete fFilename;

	if (fContext) {
		rts_ringbuffer_release(fContext);
		fContext = NULL;
	}
}

void RBVideoH265Source:: doGetNextFrame(void)
{
	int ret = 0;
	unsigned len;
	packet_t *pkt;

	fFrameSize = 0;
	ret = rts_ringbuffer_read_packet(fContext, &pkt);
	if (ret == 0) {
		if (wait_idr) {
			if (pkt->flags & RTSTREAM_PKT_FLAG_KEY)
				wait_idr = 0;
			else
				goto out;
		}

		len = pkt->length - H265_NALU_STARTCODE_LEN;
		if (len > fMaxSize) {
			fNumTruncatedBytes = len - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else {
			fFrameSize = len;
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, (char *)pkt->vm_addr + H265_NALU_STARTCODE_LEN, fFrameSize);
		gettimeofday(&fPresentationTime, NULL);
		if (fFirstTimeStamp == 0) {
			fFirstTimeStamp = 1;
			fLastPlayTime = pkt->timestamp;
			fDurationInMicroseconds = 1000000 / fFmt.video.denominator;
		} else {
			unsigned uSeconds	= fPresentationTime.tv_usec + pkt->timestamp - fLastPlayTime;
			fDurationInMicroseconds = pkt->timestamp - fLastPlayTime;
			fLastPlayTime = pkt->timestamp;
		}
out:
		rts_ringbuffer_free_packet(pkt);
	}
	envir().taskScheduler().scheduleDelayedTask(
		4000,
		(TaskFunc*)FramedSource::afterGetting,
		this);
}

///////////////////////  RingbufferAudioSource ULAW //////////////////////////////
RBAudioULAWSource *RBAudioULAWSource::createNew(
	UsageEnvironment &env,
	char const *filename)
{
	do {
		RBAudioULAWSource* newSource = new RBAudioULAWSource(env, filename);
		if (newSource != NULL && newSource->bitsPerSample() == 0) {
		Medium::close(newSource);
		break;
	}
	return newSource;
    } while (0);

	return NULL;
}

RBAudioULAWSource::RBAudioULAWSource(UsageEnvironment &env,
		const char *filename)
: FramedSource(env), fFilename(NULL), fContext(NULL), fState(False),
	fLimitNumBytesToStream(False),
	fNumBytesToStream(0),
	fNumChannels(0),
	fSamplingFrequency(0),
	fBitsPerSample(0),
	fLastPlayTime(0),
	fPlayTimePerSample(0),
	fFirstTimeStamp(0)
{
	fFilename = strDup(filename);
	fFmt.fmt = RB_AV_FMT_UNDEFINED;

	fContext = rts_ringbuffer_init(fFilename, 0);
	if (fContext) {
		rts_ringbuffer_get_stream_format(fContext, &fFmt);
		fState = True;

		fNumChannels = fFmt.audio.channels;
		fSamplingFrequency = fFmt.audio.samplerate;
		fBitsPerSample = fFmt.audio.bitfmt;
		unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
		if (bytesPerSample == 0) bytesPerSample = 1;
	}
	fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
	unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
	unsigned desiredSamplesPerFrame = (unsigned)(0.04*fSamplingFrequency);
	unsigned samplesPerFrame =
		desiredSamplesPerFrame < maxSamplesPerFrame ? desiredSamplesPerFrame : maxSamplesPerFrame;
	fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample);
}

RBAudioULAWSource::~RBAudioULAWSource(void)
{
	fState = False;
	if (fFilename)
		delete fFilename;

	if (fContext) {
		rts_ringbuffer_release(fContext);
		fContext = NULL;
	}
}

void RBAudioULAWSource:: doGetNextFrame(void)
{
	int ret = 0;
	unsigned len;
	packet_t *pkt;

	fFrameSize = 0;
	/*control stream input if fLimitNumBytesToStream on*/
	if (fLimitNumBytesToStream && fNumBytesToStream < fMaxSize) {
		fMaxSize = fNumBytesToStream;
	}
	if (fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}

	ret = rts_ringbuffer_read_packet(fContext, &pkt);
	if(ret == 0) {
		len = pkt->length;
		if (len > fMaxSize)
		{
			fNumTruncatedBytes = len - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else {
			fFrameSize = len;
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, pkt->vm_addr, fFrameSize);
		gettimeofday(&fPresentationTime, NULL);
		if (fFirstTimeStamp == 0) {
			fFirstTimeStamp = 1;
			fLastPlayTime = pkt->timestamp;
			unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
			if (bytesPerSample == 0) bytesPerSample = 1;
			fDurationInMicroseconds = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);
		} else {
			unsigned uSeconds	= fPresentationTime.tv_usec + pkt->timestamp - fLastPlayTime;
			fDurationInMicroseconds = pkt->timestamp - fLastPlayTime;
			fLastPlayTime = pkt->timestamp;
		}

	        rts_ringbuffer_free_packet(pkt);
		}

	envir().taskScheduler().scheduleDelayedTask(
		4000,
		(TaskFunc*)FramedSource::afterGetting,
		this);
}

///////////////////////  RingbufferAudioSource ALAW //////////////////////////////
RBAudioALAWSource *RBAudioALAWSource::createNew(
	UsageEnvironment &env,
	char const *filename)
{
	do {
		RBAudioALAWSource* newSource = new RBAudioALAWSource(env, filename);
		if (newSource != NULL && newSource->bitsPerSample() == 0) {
		Medium::close(newSource);
		break;
	}
	return newSource;
    } while (0);

	return NULL;
}

RBAudioALAWSource::RBAudioALAWSource(UsageEnvironment &env,
		const char *filename)
: FramedSource(env), fFilename(NULL), fContext(NULL), fState(False),
	fLimitNumBytesToStream(False),
	fNumBytesToStream(0),
	fNumChannels(0),
	fSamplingFrequency(0),
	fBitsPerSample(0),
	fLastPlayTime(0),
	fPlayTimePerSample(0),
	fFirstTimeStamp(0)
{
	fFilename = strDup(filename);
	fFmt.fmt = RB_AV_FMT_UNDEFINED;

	fContext = rts_ringbuffer_init(fFilename, 0);
	if (fContext) {
		rts_ringbuffer_get_stream_format(fContext, &fFmt);
		fState = True;

		fNumChannels = fFmt.audio.channels;
		fSamplingFrequency = fFmt.audio.samplerate;
		fBitsPerSample = fFmt.audio.bitfmt;
		unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
		if (bytesPerSample == 0) bytesPerSample = 1;
	}
	fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
	unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
	unsigned desiredSamplesPerFrame = (unsigned)(0.04*fSamplingFrequency);
	unsigned samplesPerFrame =
		desiredSamplesPerFrame < maxSamplesPerFrame ? desiredSamplesPerFrame : maxSamplesPerFrame;
	fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample);
}

RBAudioALAWSource::~RBAudioALAWSource(void)
{
	fState = False;
	if (fFilename)
		delete fFilename;

	if (fContext) {
		rts_ringbuffer_release(fContext);
		fContext = NULL;
	}
}

void RBAudioALAWSource:: doGetNextFrame(void)
{
	int ret = 0;
	unsigned len;
	packet_t *pkt;

	fFrameSize = 0;
	/*control stream input if fLimitNumBytesToStream on*/
	if (fLimitNumBytesToStream && fNumBytesToStream < fMaxSize) {
		fMaxSize = fNumBytesToStream;
	}
	if (fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}

	ret = rts_ringbuffer_read_packet(fContext, &pkt);
	if(ret == 0) {
		len = pkt->length;
		if (len > fMaxSize)
		{
			fNumTruncatedBytes = len - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else {
			fFrameSize = len;
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, pkt->vm_addr, fFrameSize);
		gettimeofday(&fPresentationTime, NULL);
		if (fFirstTimeStamp == 0) {
			fFirstTimeStamp = 1;
			fLastPlayTime = pkt->timestamp;
			unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
			if (bytesPerSample == 0) bytesPerSample = 1;
			fDurationInMicroseconds = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);
		} else {
			unsigned uSeconds	= fPresentationTime.tv_usec + pkt->timestamp - fLastPlayTime;
			fDurationInMicroseconds = pkt->timestamp - fLastPlayTime;
			fLastPlayTime = pkt->timestamp;
		}

	        rts_ringbuffer_free_packet(pkt);
		}

	envir().taskScheduler().scheduleDelayedTask(
		4000,
		(TaskFunc*)FramedSource::afterGetting,
		this);
}

///////////////////////  RingbufferAudioSource AAC //////////////////////////////
static unsigned const samplingFrequencyTable[16] = {
	96000, 88200, 64000, 48000,
	44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000,
	7350, 0, 0, 0
};

RBAudioAACSource *RBAudioAACSource::createNew(
	UsageEnvironment &env,
	char const *filename)
{
	do {
		RBAudioAACSource* newSource = new RBAudioAACSource(env, filename);
		if (newSource != NULL && newSource->bitsPerSample() == 0) {
		        Medium::close(newSource);
		        break;
	        }
	        return newSource;
        } while (0);

	return NULL;
}

void RBAudioAACSource::getAACConfigstr(uint8_t profile,
			uint32_t samplerate,
			uint8_t channels)
{
	unsigned char audioSpecificConfig[2];
	unsigned char sampleFlag;
	unsigned samplingFrequencyIndex;
	u_int8_t const audioObjectType = profile + 1;

	for(samplingFrequencyIndex = 0;
			samplingFrequencyIndex < (sizeof(samplingFrequencyTable) / sizeof(samplingFrequencyTable[0]));
			samplingFrequencyIndex++)
	{
		if (samplingFrequencyTable[samplingFrequencyIndex] == samplerate)
			sampleFlag = 1;
			break;
	}

	if(!sampleFlag) {
		envir() << "Unknown AAC Frequency " << samplerate << "\n";
		samplingFrequencyIndex = 0;
	}
	audioSpecificConfig[0] = (audioObjectType << 3) | (samplingFrequencyIndex >> 1);
	audioSpecificConfig[1] = (samplingFrequencyIndex << 7) | (channels << 3);
	sprintf(fConfigStr, "%02X%02x", audioSpecificConfig[0], audioSpecificConfig[1]);
}

RBAudioAACSource::RBAudioAACSource(UsageEnvironment &env,
		const char *filename)
: FramedSource(env), fFilename(NULL), fContext(NULL), fState(False),
	fLimitNumBytesToStream(False),
	fNumBytesToStream(0),
	fProfile(0),
	fNumChannels(0),
	fSamplingFrequency(0),
	fBitsPerSample(0),
	fLastPlayTime(0),
	fPlayTimePerSample(0),
	fFirstTimeStamp(0)
{
	fFilename = strDup(filename);
	fFmt.fmt = RB_AV_FMT_UNDEFINED;

	fContext = rts_ringbuffer_init(fFilename, 0);
	if (fContext) {
		rts_ringbuffer_get_stream_format(fContext, &fFmt);
		fState = True;

		fNumChannels = fFmt.audio.channels;
		fSamplingFrequency = fFmt.audio.samplerate;
		fBitsPerSample = fFmt.audio.bitfmt;
		unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
		if (bytesPerSample == 0) bytesPerSample = 1;
	}
	fPlayTimePerSample = 1e6/(double)fSamplingFrequency;
	unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
	unsigned desiredSamplesPerFrame = (unsigned)(0.04*fSamplingFrequency);
	unsigned samplesPerFrame = desiredSamplesPerFrame < maxSamplesPerFrame ? desiredSamplesPerFrame : maxSamplesPerFrame;
	fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample);


	getAACConfigstr(fProfile, fSamplingFrequency, fNumChannels);
}

RBAudioAACSource::~RBAudioAACSource(void)
{
	fState = False;
	if (fFilename)
		delete fFilename;

	if (fContext) {
		rts_ringbuffer_release(fContext);
		fContext = NULL;
	}
}

void RBAudioAACSource:: doGetNextFrame(void)
{
	int ret = 0;
	unsigned len;
	packet_t *pkt;

	fFrameSize = 0;
	/*Control Stream Input if fLimitNumBytesToStream ON*/
	if (fLimitNumBytesToStream && fNumBytesToStream < fMaxSize) {
		fMaxSize = fNumBytesToStream;
	}
	if (fPreferredFrameSize < fMaxSize) {
		fMaxSize = fPreferredFrameSize;
	}

	ret = rts_ringbuffer_read_packet(fContext, &pkt);
	if(ret == 0) {
		len = pkt->length;
		if (len > fMaxSize)
		{
			fNumTruncatedBytes = len - fMaxSize;
			fFrameSize = fMaxSize;
		}
		else {
			fFrameSize = len;
			fNumTruncatedBytes = 0;
		}

		memcpy(fTo, pkt->vm_addr, fFrameSize);
		gettimeofday(&fPresentationTime, NULL);
		if (fFirstTimeStamp == 0) {
			fFirstTimeStamp = 1;
			fLastPlayTime = pkt->timestamp;
			unsigned bytesPerSample = (fNumChannels*fBitsPerSample)/8;
			if (bytesPerSample == 0) bytesPerSample = 1;
			fDurationInMicroseconds = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);
		} else {
			unsigned uSeconds	= fPresentationTime.tv_usec + pkt->timestamp - fLastPlayTime;
			fDurationInMicroseconds = pkt->timestamp - fLastPlayTime;
			fLastPlayTime = pkt->timestamp;
		}

	rts_ringbuffer_free_packet(pkt);
		}

	envir().taskScheduler().scheduleDelayedTask(
		4000,
		(TaskFunc*)FramedSource::afterGetting,
		this);
}
///////////////////////  RBServerVideoSubsession  H264//////////////////////////////

RBVideoH264Subsession *RBVideoH264Subsession::createNew(
	UsageEnvironment &env,
	const char *filename,
	Boolean reuseFirstSource)
{
	return new RBVideoH264Subsession(env, filename,
							reuseFirstSource);
}

RBVideoH264Subsession::RBVideoH264Subsession(
		UsageEnvironment &env,
		const char *filename,
		Boolean reuseFirstSource)
:FileServerMediaSubsession(env, filename, reuseFirstSource)
{
	char *dst = NULL;
	char filename_buf[64];

	memcpy(filename_buf, filename, strlen(filename));
	dst = strstr(filename_buf, ".shm");
	if (dst != NULL) {
		*dst = '\0';
		RBVideoH264Subsession::profile_id = atoi(dst-1);
	}
}

FramedSource *RBVideoH264Subsession::createNewStreamSource(
		unsigned clientSessionId,
		unsigned &estBitrate
		)
{
	estBitrate = 1000;
	RBVideoH264Source *streamSource = RBVideoH264Source::createNew(envir(), fFileName);
	if (streamSource)
		return H264VideoStreamDiscreteFramer::createNew(envir(), streamSource);

	return NULL;
}

RTPSink *RBVideoH264Subsession::createNewRTPSink(
		Groupsock *rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *inputSource
		)
{
	OutPacketBuffer::maxSize = 500000;

	int i;
	int video_index = -1;
	h26x_parse_info info = {0};
	int ret = -1;
	void *handle =NULL;
	void *buf = NULL;
	void *buf_offset = NULL;
	packet_t *pkt;

	video_index = RBVideoH264Subsession::profile_id - 1;

	if (access(H26X_INFO_SHARE_FILE, F_OK) == 0) {
		handle = rts_ringbuffer_init(H26X_INFO_SHARE_FILE, 0);
		if (handle != NULL) {
			ret = rts_ringbuffer_read_packet(handle, &pkt);
			if (ret == 0)
				buf = pkt->vm_addr;
		}
		if (buf != NULL) {
			buf_offset = buf + video_index * 512 + ENCODE_TYPE_H264 * 256;
			memcpy(&info, buf_offset, sizeof(h26x_parse_info));
		}
		if (pkt != NULL)
			rts_ringbuffer_free_packet(pkt);
		if (handle != NULL)
			rts_ringbuffer_release(handle);

		return H264VideoRTPSink::createNew(
				envir(),
				rtpGroupsock,
				rtpPayloadTypeIfDynamic,
				info.sps, info.sps_len,
				info.pps, info.pps_len
				);
	} else
		return H264VideoRTPSink::createNew(
				envir(),
				rtpGroupsock,
				rtpPayloadTypeIfDynamic
				);
}

///////////////////////  RBServerVideoSubsession  H265//////////////////////////////

RBVideoH265Subsession *RBVideoH265Subsession::createNew(
	UsageEnvironment &env,
	const char *filename,
	Boolean reuseFirstSource)
{
	return new RBVideoH265Subsession(env, filename,
							reuseFirstSource);
}

RBVideoH265Subsession::RBVideoH265Subsession(
		UsageEnvironment &env,
		const char *filename,
		Boolean reuseFirstSource)
:FileServerMediaSubsession(env, filename, reuseFirstSource)
{
	char *dst = NULL;
	char filename_buf[64];

	memcpy(filename_buf, filename, strlen(filename));
	dst = strstr(filename_buf, ".shm");
	if (dst != NULL) {
		*dst = '\0';
		RBVideoH265Subsession::profile_id = atoi(dst-1);
	}
}

FramedSource *RBVideoH265Subsession::createNewStreamSource(
		unsigned clientSessionId,
		unsigned &estBitrate
		)
{
	estBitrate = 500;
	RBVideoH265Source *streamSource = RBVideoH265Source::createNew(envir(), fFileName);
	if (streamSource)
		return H265VideoStreamDiscreteFramer::createNew(envir(), streamSource);

	return NULL;
}

RTPSink *RBVideoH265Subsession::createNewRTPSink(
		Groupsock *rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *inputSource
		)
{
	OutPacketBuffer::maxSize = 500000;
	int i;
	int video_index = -1;
	h26x_parse_info info = {0};
	int ret = -1;
	void *handle =NULL;
	void *buf = NULL;
	void *buf_offset = NULL;
	packet_t *pkt;

	video_index = RBVideoH265Subsession::profile_id - 1;

	if (access(H26X_INFO_SHARE_FILE, F_OK) == 0) {
		handle = rts_ringbuffer_init(H26X_INFO_SHARE_FILE, 0);
		if (handle != NULL) {
			ret = rts_ringbuffer_read_packet(handle, &pkt);
			if (ret == 0)
				buf = pkt->vm_addr;
		}
		if (buf != NULL) {
			buf_offset = buf + video_index * 512 + ENCODE_TYPE_H265 * 256;
			memcpy(&info, buf_offset, sizeof(h26x_parse_info));
		}
		if (pkt != NULL)
			rts_ringbuffer_free_packet(pkt);
		if (handle != NULL)
			rts_ringbuffer_release(handle);

		return H265VideoRTPSink::createNew(
				envir(),
				rtpGroupsock,
				rtpPayloadTypeIfDynamic,
				info.vps, info.vps_len,
				info.sps, info.sps_len,
				info.pps, info.pps_len
				);
	} else
		return H265VideoRTPSink::createNew(
				envir(),
				rtpGroupsock,
				rtpPayloadTypeIfDynamic
				);
}

///////////////////////  RBServerAudioSubsession  ULAW//////////////////////////////

RBAudioULAWSubsession *RBAudioULAWSubsession::createNew(
	UsageEnvironment &env,
	const char *filename,
	Boolean reuseFirstSource)
{
	return new RBAudioULAWSubsession(env, filename,
							reuseFirstSource);
}

RBAudioULAWSubsession::RBAudioULAWSubsession(
		UsageEnvironment &env,
		const char *filename,
		Boolean reuseFirstSource)
:FileServerMediaSubsession(env, filename, reuseFirstSource)
{
}

FramedSource *RBAudioULAWSubsession::createNewStreamSource(
		unsigned clientSessionId,
		unsigned &estBitrate
		)
{
	FramedSource* resultSource = NULL;
	do {
		RBAudioULAWSource *streamSource = RBAudioULAWSource::createNew(envir(), fFileName);
		if (streamSource == NULL) break;

		// Get attributes of the audio source:

		fBitsPerSample = streamSource->bitsPerSample();
		if (!(fBitsPerSample == 4 || fBitsPerSample == 8 || fBitsPerSample == 16)) {
			envir() << "The input file contains " << fBitsPerSample << " bit-per-sample	audio, which we don't handle\n";
			break;
		}
		fSamplingFrequency = streamSource->samplingFrequency();
		fNumChannels = streamSource->numChannels();
		unsigned bitsPerSecond
			= fSamplingFrequency*fBitsPerSample*fNumChannels;

		resultSource = streamSource;

		estBitrate = (bitsPerSecond+500)/1000; // kbps
		return resultSource;
	} while (0);

	// An error occurred:
	Medium::close(resultSource);
	return NULL;
}

RTPSink *RBAudioULAWSubsession::createNewRTPSink(
		Groupsock *rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *inputSource
		)
{
	RBAudioULAWSource* ULAWSource = (RBAudioULAWSource*)inputSource;
	stream_format fmt = ULAWSource->getFormat();
	const char *mimeType = "PCMU";
	unsigned char payloadFormatCode;

	if (fSamplingFrequency == 8000 && fNumChannels == 1) {
		payloadFormatCode = 0; // a static RTP payload type
	} else {
		payloadFormatCode = rtpPayloadTypeIfDynamic;
	}
	return SimpleRTPSink::createNew(
			envir(),
			rtpGroupsock,
			payloadFormatCode,
			fSamplingFrequency, //frequency
			"audio",
			mimeType,
			fNumChannels
			);
}

///////////////////////  RBServerAudioSubsession  ALAW//////////////////////////////
RBAudioALAWSubsession *RBAudioALAWSubsession::createNew(
	UsageEnvironment &env,
	const char *filename,
	Boolean reuseFirstSource)
{
	return new RBAudioALAWSubsession(env, filename,
							reuseFirstSource);
}

RBAudioALAWSubsession::RBAudioALAWSubsession(
		UsageEnvironment &env,
		const char *filename,
		Boolean reuseFirstSource)
:FileServerMediaSubsession(env, filename, reuseFirstSource)
{
}

FramedSource *RBAudioALAWSubsession::createNewStreamSource(
		unsigned clientSessionId,
		unsigned &estBitrate
		)
{
	FramedSource* resultSource = NULL;
	do {
		RBAudioALAWSource *streamSource = RBAudioALAWSource::createNew(envir(), fFileName);
		if (streamSource == NULL) break;

		// Get attributes of the audio source:
		fBitsPerSample = streamSource->bitsPerSample();
		if (!(fBitsPerSample == 4 || fBitsPerSample == 8 || fBitsPerSample == 16)) {
			envir() << "The input file contains " << fBitsPerSample
				<< " bit-per-sample	audio, which we don't handle\n";
			break;
		}
		fSamplingFrequency = streamSource->samplingFrequency();
		fNumChannels = streamSource->numChannels();
		unsigned bitsPerSecond = fSamplingFrequency*fBitsPerSample*fNumChannels;

		resultSource = streamSource;

		estBitrate = (bitsPerSecond+500)/1000; // kbps
		return resultSource;
	} while (0);

	// An error occurred:
	Medium::close(resultSource);
	return NULL;
}

RTPSink *RBAudioALAWSubsession::createNewRTPSink(
		Groupsock *rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *inputSource
		)
{
	RBAudioALAWSource* ALAWSource = (RBAudioALAWSource*)inputSource;
	stream_format fmt = ALAWSource->getFormat();
	const char *mimeType = "PCMA";
	unsigned char payloadFormatCode;

	if (fSamplingFrequency == 8000 && fNumChannels == 1) {
		payloadFormatCode = 8; // a static RTP payload type
	} else {
		payloadFormatCode = rtpPayloadTypeIfDynamic;
	}
	return SimpleRTPSink::createNew(
			envir(),
			rtpGroupsock,
			payloadFormatCode,
			fSamplingFrequency, //frequency
			"audio",
			mimeType,
			fNumChannels
			);
}

///////////////////////  RBServerAudioSubsession  AAC//////////////////////////////
RBAudioAACSubsession *RBAudioAACSubsession::createNew(
	UsageEnvironment &env,
	const char *filename,
	Boolean reuseFirstSource)
{
	return new RBAudioAACSubsession(env, filename,
							reuseFirstSource);
}

RBAudioAACSubsession::RBAudioAACSubsession(
		UsageEnvironment &env,
		const char *filename,
		Boolean reuseFirstSource)
:FileServerMediaSubsession(env, filename, reuseFirstSource)
{
}

FramedSource *RBAudioAACSubsession::createNewStreamSource(
		unsigned clientSessionId,
		unsigned &estBitrate
		)
{
	estBitrate = 96;
	FramedSource* resultSource = NULL;
	do {
		RBAudioAACSource *streamSource = RBAudioAACSource::createNew(envir(), fFileName);
		if (streamSource == NULL) break;

		fBitsPerSample = streamSource->bitsPerSample();
		fSamplingFrequency = streamSource->samplingFrequency();
		fNumChannels = streamSource->numChannels();

		resultSource = streamSource;

		return resultSource;
	} while (0);
	// An error occurred:
	Medium::close(resultSource);
	return NULL;
}

RTPSink *RBAudioAACSubsession::createNewRTPSink(
		Groupsock *rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource *inputSource
		)
{
	RBAudioAACSource* AACSource = (RBAudioAACSource*)inputSource;
	stream_format fmt = AACSource->getFormat();
	const char *mimeType = "AAC-hbr";

	return MPEG4GenericRTPSink::createNew(envir(),
			rtpGroupsock,
			rtpPayloadTypeIfDynamic,
			fSamplingFrequency,
			"audio",
			"AAC-hbr",
			AACSource->configStr(),
			fNumChannels
			);
}
///////////////////////  RingbufferRTSPServer  //////////////////////////////

RingbufferRTSPServer::~RingbufferRTSPServer()
{
}

RingbufferRTSPServer::RingbufferRTSPServer(
	UsageEnvironment& env, int ourSocketIPv4, int ourSocketIPv6,
	Port ourPort,
	UserAuthenticationDatabase* authDatabase, unsigned reclamationTestSeconds)
: RTSPServer(env, ourSocketIPv4, ourSocketIPv6, ourPort, authDatabase, reclamationTestSeconds) {
}

RingbufferRTSPServer *
RingbufferRTSPServer::createNew(UsageEnvironment& env, Port ourPort,
			     UserAuthenticationDatabase* authDatabase,
			     unsigned reclamationTestSeconds) {
  int ourSocketIPv4 = setUpOurSocket(env, ourPort, AF_INET);
  int ourSocketIPv6 = setUpOurSocket(env, ourPort, AF_INET6);
  if (ourSocketIPv4 < 0 && ourSocketIPv6 < 0) return NULL;

  return new RingbufferRTSPServer(env, ourSocketIPv4, ourSocketIPv6, ourPort,
			       authDatabase, reclamationTestSeconds);
}

Boolean RingbufferRTSPServer::isRingbufferExist(char *filename)
{
	void *rc = rts_ringbuffer_init(filename, 0);
	Boolean r = True;

	if (!rc)
		r = False;

	rts_ringbuffer_release(rc);
	return r;
}

#define RINGBUFFER_VIDEO_FILENAME_FORMAT "/var/tmp/capture_video_%s.shm"

void RingbufferRTSPServer::lookupServerMediaSession(char const* streamName,
			   lookupServerMediaSessionCompletionFunc* completionFunc,
			   void* completionClientData,
			   Boolean isFirstLookupInSession)
{
	char shm_video[64] = {0};
	char shm_audio[64] = "/var/tmp/capture_audio_profile.shm";
	Boolean video_exist = 0, audio_exist = 0;
	ServerMediaSubsession *video_ss = NULL, *audio_ss = NULL;

	snprintf(shm_video, sizeof(shm_video), RINGBUFFER_VIDEO_FILENAME_FORMAT,
				streamName);
	video_exist = RingbufferRTSPServer::isRingbufferExist(shm_video);
	audio_exist = RingbufferRTSPServer::isRingbufferExist(shm_audio);

	// Next, check whether we already have a "ServerMediaSession" for this file:
	ServerMediaSession* sms = getServerMediaSession(streamName);

	if (!sms && (video_exist || audio_exist)) {
		sms = ServerMediaSession::createNew(envir(), streamName,
						streamName,
						"realtek rtsp server(live555)");
		if (!sms)
			goto failed;
		if (video_exist) {
			fVideoContext = rts_ringbuffer_init(shm_video, 0);
			if (fVideoContext)
				rts_ringbuffer_get_stream_format(fVideoContext, &fVideoFmt);
			switch(fVideoFmt.fmt)
			{
				case RB_V_FMT_H264:
				video_ss = RBVideoH264Subsession::createNew(
						envir(),
						shm_video,
						True);
				sms->addSubsession(video_ss);
				break;

				case RB_V_FMT_H265:
				video_ss = RBVideoH265Subsession::createNew(
						envir(),
						shm_video,
						True);
				sms->addSubsession(video_ss);
				break;

				default:
				goto failed;
			}
		}

		if (audio_exist) {
			fAudioContext = rts_ringbuffer_init(shm_audio, 0);
			if (fAudioContext)
				rts_ringbuffer_get_stream_format(fAudioContext, &fAudioFmt);
			switch(fAudioFmt.fmt)
			{
				case RB_A_FMT_ULAW:
				audio_ss = RBAudioULAWSubsession::createNew(
						envir(),
						shm_audio,
						True);
				sms->addSubsession(audio_ss);
				break;

				case RB_A_FMT_ALAW:
				audio_ss = RBAudioALAWSubsession::createNew(
						envir(),
						shm_audio,
						True);
				sms->addSubsession(audio_ss);
				break;

				case RB_A_FMT_AAC:
				audio_ss = RBAudioAACSubsession::createNew(
						envir(),
						shm_audio,
						True);
				sms->addSubsession(audio_ss);
				break;

				default:
				goto failed;
			}
		}

		addServerMediaSession(sms);
	}

	if (completionFunc != NULL) {
		(*completionFunc)(completionClientData, sms);
	}
	return;

failed:
	if (audio_ss)
		close(audio_ss);
	if (video_ss)
		close(video_ss);
	if (sms)
		removeServerMediaSession(sms);
	return;
}

///////////////////////  main  //////////////////////////////

int main(int argc, const char *argv[])
{
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

	RTSPServer* rtspServer;
	int portnum;
	portNumBits rtspServerPortNum;
	portnum = update_rtspServerPortNum_from_json();
	if (portnum == -1)
		rtspServerPortNum = 554;
	else
		rtspServerPortNum = (portNumBits)portnum;

	rtspServer = RingbufferRTSPServer::createNew(*env,
						rtspServerPortNum,
						NULL);
	if (NULL == rtspServer) {
		rtspServerPortNum = 8554;

		rtspServer = RingbufferRTSPServer::createNew(*env,
				rtspServerPortNum,
				NULL);
	}
	if (rtspServer == NULL) {
		*env << "Failed to create RTSP server: "
			<< env->getResultMsg() << "\n";
		exit(1);
	}
	char* urlPrefix = rtspServer->rtspURLPrefix();
	*env << "Play streams from this server using the URL\n\t"
		<< urlPrefix << "<filename>\n where <filename> is a file present in the current directory.\n";

	*env << "stream Port:" << rtspServerPortNum << "\n";

	if (rtspServer->setUpTunnelingOverHTTP(8000)
			|| rtspServer->setUpTunnelingOverHTTP(8080)
			|| rtspServer->setUpTunnelingOverHTTP(80)) {
		*env << "(We use port " << rtspServer->httpServerPortNum()
			<< " for optional RTSP-over-HTTP tunneling.)\n";
	} else {
		*env << "(RTSP-over-HTTP tunneling is not available.)\n";
	}

	env->taskScheduler().doEventLoop();

	return 0;
}
