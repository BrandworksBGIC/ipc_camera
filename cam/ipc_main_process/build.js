// Read configuration
const cloud = config.getOrDefault('cloud', 'instaview');
const platform = config.getOrDefault('platform', 'rts3917');
const ipcVersion = config.getOrDefault('ipc_version', '1.0.0');
const abortDiagnosticsConfig = config.getOrDefault('instaview_abort_diagnostics', false);
const enableInstaviewAbortDiagnostics = abortDiagnosticsConfig === true
    || abortDiagnosticsConfig === 'true' || abortDiagnosticsConfig === '1';

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

if (cloud === 'instaview' && enableInstaviewAbortDiagnostics) {
    mainProcess.addLdflags('-ldl');
    // Opt-in crash diagnostics. Release firmware keeps this disabled by default.
    mainProcess.addCflags('-DIPC_INSTAVIEW_ABORT_DIAGNOSTICS=1');
    mainProcess.addCflags('-DIPC_INSTAVIEW_SDK_BUILD_ID="7425f265a0dd9d2084530238296fe4dcc9590a5d"');
    mainProcess.addLdflags(`-Wl,-Map=build/${platform}/t_ipc/ipc.abort-diagnostics.map`);
    for (const symbol of ['abort', '__assert', '__stack_chk_fail']) {
        mainProcess.addLdflags(`-Wl,--undefined=${symbol}`);
        mainProcess.addLdflags(`-Wl,--export-dynamic-symbol=${symbol}`);
    }
}

// Add source files
mainProcess.addFiles('*.c');

// Build main process
if (mainProcess.build(true)) {
    console.log('Main process build successful');
} else {
    console.error('Main process build failed');
    process.exit(1);
}
