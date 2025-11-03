/*
 *  Copyright (C) 2019 Realtek Semiconductor Corp.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RTS_RTSPD_H__
#define __RTS_RTSPD_H__
#include <ringbuffer.h>
#include <FramedSource.hh>
#include <FileServerMediaSubsession.hh>
#include <Media.hh>
#include <AMRAudioSource.hh>

/*******************Video Source**************************/
class RBVideoH264Source: public FramedSource {
public:
	static RBVideoH264Source *createNew(
			UsageEnvironment &env,
			char const *filename
			);
	stream_format getFormat(void) { return fFmt; }
	virtual void doGetNextFrame(void);

protected:

	RBVideoH264Source(UsageEnvironment &env, const char *filename);
	virtual ~RBVideoH264Source(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	uint64_t fFirstTimeStamp;
	uint64_t fLastPlayTime;
	int wait_idr;
};

class RBVideoH265Source: public FramedSource {
public:
	static RBVideoH265Source *createNew(
			UsageEnvironment &env,
			char const *filename
			);
	stream_format getFormat(void) { return fFmt; }
	virtual void doGetNextFrame(void);

protected:

	RBVideoH265Source(UsageEnvironment &env, const char *filename);
	virtual ~RBVideoH265Source(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	uint64_t fFirstTimeStamp;
	uint64_t fLastPlayTime;
	int wait_idr;
};

/*******************Audio Source**************************/
class RBAudioULAWSource: public FramedSource {
public:
	static RBAudioULAWSource *createNew(
			UsageEnvironment &env,
			char const *filename
			);

	stream_format getFormat(void) { return fFmt; }

	unsigned bitsPerSample() const { return fBitsPerSample; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned samplingFrequency() const { return fSamplingFrequency; }

protected:

	RBAudioULAWSource(UsageEnvironment &env, const char *filename);
	virtual ~RBAudioULAWSource(void);

private:
	virtual void doGetNextFrame(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	Boolean fLimitNumBytesToStream;
	unsigned fNumBytesToStream;
	unsigned fNumChannels;
	unsigned fSamplingFrequency;
	unsigned fBitsPerSample;
	unsigned fPreferredFrameSize;
	unsigned fLastPlayTime;
	unsigned fPlayTimePerSample;
	uint64_t fFirstTimeStamp;

};

class RBAudioALAWSource: public FramedSource {
public:
	static RBAudioALAWSource *createNew(
			UsageEnvironment &env,
			char const *filename
			);

	stream_format getFormat(void) { return fFmt; }

	unsigned bitsPerSample() const { return fBitsPerSample; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned samplingFrequency() const { return fSamplingFrequency; }


protected:

	RBAudioALAWSource(UsageEnvironment &env, const char *filename);
	virtual ~RBAudioALAWSource(void);

private:
	virtual void doGetNextFrame(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	Boolean fLimitNumBytesToStream;
	unsigned fNumBytesToStream;
	unsigned fNumChannels;
	unsigned fSamplingFrequency;
	unsigned fBitsPerSample;
	unsigned fPreferredFrameSize;
	unsigned fLastPlayTime;
	unsigned fPlayTimePerSample;
	uint64_t fFirstTimeStamp;
};

class RBAudioAMRSource: public FramedSource {
public:
	static RBAudioAMRSource *createNew(
			UsageEnvironment &env,
			char const *filename
			);

	stream_format getFormat(void) { return fFmt; }

	unsigned bitsPerSample() const { return fBitsPerSample; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned samplingFrequency() const { return fSamplingFrequency; }


protected:

	RBAudioAMRSource(UsageEnvironment &env, const char *filename);
	virtual ~RBAudioAMRSource(void);

private:
	virtual void doGetNextFrame(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	Boolean fLimitNumBytesToStream;
	unsigned fNumBytesToStream;
	unsigned fNumChannels;
	unsigned fSamplingFrequency;
	unsigned fBitsPerSample;
	unsigned fPreferredFrameSize;
	unsigned fLastPlayTime;
	unsigned fPlayTimePerSample;
	uint64_t fFirstTimeStamp;
};

class RBAudioAACSource: public FramedSource {
public:
	static RBAudioAACSource *createNew(
			UsageEnvironment &env,
			char const *filename
			);

	void getAACConfigstr(uint8_t profile,
			uint32_t samplerate,
			uint8_t channels
			);

	stream_format getFormat(void) { return fFmt; }

	unsigned bitsPerSample() const { return fBitsPerSample; }
	unsigned numChannels() const { return fNumChannels; }
	unsigned samplingFrequency() const { return fSamplingFrequency; }
	char const* configStr() const { return fConfigStr; }

protected:

	RBAudioAACSource(UsageEnvironment &env, const char *filename);
	virtual ~RBAudioAACSource(void);

private:
	virtual void doGetNextFrame(void);

private:
	char *fFilename;
	void *fContext;
	stream_format fFmt;
	Boolean fState;
	Boolean fLimitNumBytesToStream;
	unsigned fNumBytesToStream;
	unsigned fProfile;
	unsigned fNumChannels;
	unsigned fSamplingFrequency;
	unsigned fBitsPerSample;
	unsigned fPreferredFrameSize;
	unsigned fLastPlayTime;
	unsigned fPlayTimePerSample;
	uint64_t fFirstTimeStamp;
	char fConfigStr[5];
};

/*******************Video Subsession**************************/
class RBVideoH264Subsession: public FileServerMediaSubsession {
public:
	static RBVideoH264Subsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
	int profile_id;

protected:
	virtual ~RBVideoH264Subsession(void) {};
	RBVideoH264Subsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);
};

class RBVideoH265Subsession: public FileServerMediaSubsession {
public:
	static RBVideoH265Subsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
	int profile_id;

protected:
	virtual ~RBVideoH265Subsession(void) {};
	RBVideoH265Subsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);
};

/*******************Audio Subsession**************************/
class RBAudioULAWSubsession: public  FileServerMediaSubsession{
public:
	static RBAudioULAWSubsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual ~RBAudioULAWSubsession(void) {};
	RBAudioULAWSubsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);

protected:
  // "createNewStreamSource" is called:
	unsigned char fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
};

class RBAudioALAWSubsession: public FileServerMediaSubsession {
public:
	static RBAudioALAWSubsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual ~RBAudioALAWSubsession(void) {};
	RBAudioALAWSubsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);

protected:
  // The following parameters of the input stream are set after
  // "createNewStreamSource" is called:
	unsigned char fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
};

class RBAudioAMRSubsession: public FileServerMediaSubsession {
public:
	static RBAudioAMRSubsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual ~RBAudioAMRSubsession(void) {};
	RBAudioAMRSubsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);
protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);

protected:
  // The following parameters of the input stream are set after
  // "createNewStreamSource" is called:
	unsigned char fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
	Boolean isWideband;
};

class RBAudioAACSubsession: public  FileServerMediaSubsession{
public:
	static RBAudioAACSubsession *createNew(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual ~RBAudioAACSubsession(void) {};
	RBAudioAACSubsession(
			UsageEnvironment &env,
			const char *filename,
			Boolean reuseFirstSource
			);

protected:
	virtual FramedSource *createNewStreamSource(
			unsigned clientSessionId,
			unsigned &estBitrate
			);
	virtual RTPSink *createNewRTPSink(
			Groupsock *rtpGroupsock,
			unsigned char rtpPayloadTypeIfDynamic,
			FramedSource *inputSource
			);

protected:
  // "createNewStreamSource" is called:
	unsigned char fBitsPerSample;
	unsigned fSamplingFrequency;
	unsigned fNumChannels;
};

/*******************RTSP Server**************************/
class RingbufferRTSPServer : public RTSPServer {
public:
	static RingbufferRTSPServer *createNew(
				UsageEnvironment& env,
				Port ourPort,
				UserAuthenticationDatabase* authDatabase,
				unsigned reclamationTestSeconds = 65
				);
	static Boolean isRingbufferExist(char *filename);

protected:
	virtual ~RingbufferRTSPServer(void);
	RingbufferRTSPServer(UsageEnvironment& env, int ourSocketIPv4, int ourSocketIPv6, Port ourPort,
				  UserAuthenticationDatabase* authDatabase, unsigned reclamationTestSeconds);
	virtual void lookupServerMediaSession(char const* streamName,
					lookupServerMediaSessionCompletionFunc* completionFunc,
					void* completionClientData,
					Boolean isFirstLookupInSession);

private:
	void *fVideoContext, *fAudioContext;
	stream_format fVideoFmt, fAudioFmt;
};

#endif

