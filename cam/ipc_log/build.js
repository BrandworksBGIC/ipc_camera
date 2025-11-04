// Create log target, depends on core library
const log = createTarget('log', '../ipc_core/');

// Set target type as executable
log.setTargetType('executable');

// Add source files
log.addFiles('*.c');

// Add linker flags
log.addLdflags('-lpthread');

// Build log module
if (log.build()) {
    console.log('Log module build successful');
} else {
    console.error('Log module build failed');
    process.exit(1);
}