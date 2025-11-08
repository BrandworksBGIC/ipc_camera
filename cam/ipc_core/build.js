// Create core library target, depends on cJSON library
const core = createTarget('ipc_core', '../../third/cjson/');

const kind = config.getOrDefault('library_kind', "shared")
const openssl_dir = config.get('openssl_dir')


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
core.addIncludeDirs(`${openssl_dir}/include`);

// Add OpenSSL shared library
core.addLdfiles(`${openssl_dir}/libcrypto.so`);

// Build core library
if (core.build()) {
    console.log('ipc_core build successful');
} else {
    console.error('ipc_core build failed');
    process.exit(1);
}