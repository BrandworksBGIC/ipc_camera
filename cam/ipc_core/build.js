// Create core library target, depends on cJSON library
const core = createTarget('ipc_core', '../../third/cjson/');

const kind = config.getOrDefault('library_kind', "shared")


core.setTargetType(kind);

// Add source files
core.addFiles('ipc_osal/src/*.c');
core.addFiles('ipc_tool/src/*.c');
core.addFiles('ipc_tool/aes/*.c');
// core.addFiles('test/*.c');  // Commented out test files

// Add include directories, set as public export
core.addIncludeDirs(
    'ipc_osal/include/',
    'ipc_tool/include/',
    'ipc_tool/aes/',
    '.',
    true  // export = true
);

// Add OpenSSL include directories
core.addIncludeDirs('../../chip/rts3917/sdk/rts39xx_sdk_v5.3/out/rts3917n_base/build/libopenssl-custom/include');

// Add OpenSSL shared library
core.addLdfiles('../../chip/rts3917/sdk/rts39xx_sdk_v5.3/out/rts3917n_base/build/libopenssl-custom/libcrypto.so');

// Build core library
if (core.build()) {
    console.log('ipc_core build successful');
} else {
    console.error('ipc_core build failed');
    process.exit(1);
}