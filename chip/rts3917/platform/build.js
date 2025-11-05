// Create platform static library target
const platform = createTarget('ipc_platform','../../../cam/ipc_core/');

// Add source files
platform.addFiles('*/src/*.c');

// Set target type as static library
platform.setTargetType('static');

// Add basic include directories
platform.addIncludeDirs(
    'alarm/include',
    'audio/include',
    'io/include',
    'misc/include',
    'video/include',
);

// SDK path configuration
const sdk = '../sdk/rts39xx_sdk_v5.3/';

// Add SDK include directories
platform.addIncludeDirs(
    sdk + '/platform/source/rtscore/rtstream/rts3917/rtstream/usr/include',
    sdk + '/platform/source/rtscore/rtstream/rts3917/rtstream/usr/include/vpucodec',
    sdk + '/platform/source/rtscore/rtstream/rts3917/rtstream/usr/include/isp-proto',
    sdk + '/platform/source/kernel/linux-6.6/drivers/media/platform/rts_camera/linux',
    sdk + '/platform/source/rtscore/librtsio/include',
    '../AI/rtsnn_odtiny_v1.1_2022.10.07_mnn/include',
    '../../../cam/driver/ipc_gpio_driver/',
    '../../../cam/driver/m433_driver/',
);

// Add shared library link files
platform.addLdfiles(
    sdk + '/platform/source/rtscore/rtstream/rts3917/rtstream/usr/lib/*.so',
    '../AI/rtsnn_odtiny_v1.1_2022.10.07_mnn/lib/lib*.so',
    '../third_lib/*.so'
);

// Build target
if (platform.build()) {
    console.log('ipc_platform static library build successful');
} else {
    console.error('ipc_platform static library build failed');
    process.exit(1);
}

platform.addInstallFiles("lib", "../third_lib/libasound.so.2")