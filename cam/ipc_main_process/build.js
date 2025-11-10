// Read configuration
const cloud = config.getOrDefault('cloud', 'instaview');
const platform = config.getOrDefault('platform', 'rts3917');
const ipcVersion = config.getOrDefault('ipc_version', '1.0.0');

// Create main process target, depends on cloud service and platform
const mainProcess = createTarget('ipc',
    `../cloud/${cloud}`,       // Common cloud service dependency
    `../../chip/${platform}/platform`
);

// Set target type as executable
mainProcess.setTargetType('executable');

// Add compilation flags
mainProcess.addCflags(`-D__CLOUD__="${cloud}"`);
mainProcess.addCflags(`-D__IPC_VERSION__="${ipcVersion}"`);

// Add source files
mainProcess.addFiles('*.c');

// Build main process
if (mainProcess.build(true)) {
    console.log('Main process build successful');
} else {
    console.error('Main process build failed');
    process.exit(1);
}