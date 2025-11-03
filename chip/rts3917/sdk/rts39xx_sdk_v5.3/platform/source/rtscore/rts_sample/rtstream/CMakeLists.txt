
if (CONFIG_SAMPLE_RTSTREAM_VIDEO MATCHES "y")
if (CONFIG_RTSTREAM_VIDEO MATCHES "y")
	add_subdirectory(video)
endif()
endif()

if (CONFIG_SAMPLE_RTSTREAM_AUDIO MATCHES "y")
if (CONFIG_RTSTREAM_AUDIO MATCHES "y")
	add_subdirectory(audio)
endif()
endif()
