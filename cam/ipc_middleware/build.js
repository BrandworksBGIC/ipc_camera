const fs = require('fs');
const arch = config.get('arch')

// Create IPC library target, depends on multiple libraries
const ipc = createTarget('ipc_middleware',
    '../ipc_core',           // Common dependency
    '../../third/libwave/',
    '../../third/ipc_vad',
    '../../third/ed25519'
);

// Read configuration
ipc.addCflags('-D__IPC_ARCH__=\"'+arch.toUpperCase()+'\"')


const kind = config.getOrDefault('library_kind', "shared")
ipc.setTargetType(kind);


// Add source files
ipc.addFiles('src/*.c');
ipc.addFiles('src/internel/*.c');

// Add common include directories
ipc.addIncludeDirs('include/', true);

// Add other include directories
ipc.addIncludeDirs(
    '../../ipc/platform/',
    'include/internel',
    '../driver/ipc_driver_motor/',
    "../driver/m433_driver/"
);


ipc.addFiles2Hex('script/*');  

// Generate configuration file before build
generateConfigHeader();

// Build IPC library
if (ipc.build()) {
    console.log('ipc_middleware build successful');
} else {
    console.error('ipc_middleware build failed');
    process.exit(1);
}

// Generate configuration header file
function generateConfigHeader() {
    const ipcConfig = `#ifndef __IPC_MIDDLEWARE_CONFIG_H__
#define __IPC_MIDDLEWARE_CONFIG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <ipc_core.h>

%s

#ifdef __cplusplus
}
#endif

#endif //__IPC_MIDDLEWARE_CONFIG_H__
`;

    const desc = ipcConfig.replace('%s', '');
    const configPath = 'include/ipc_middleware_config.h';

    try {
        if (fs.existsSync(configPath)) {
            const prev = fs.readFileSync(configPath, 'utf8');
            if (prev === desc) {
                console.log('Configuration file unchanged');
                return;
            }
        }

        fs.writeFileSync(configPath, desc, 'utf8');
        console.log('Configuration file generated');
    } catch (error) {
        console.error('Failed to generate configuration file:', error);
    }
}