// Create daemon target, depends on core library
const daemon = createTarget('daemon', '../ipc_core/');

// Set target type as executable
daemon.setTargetType('executable');

// Add source files
daemon.addFiles('*.c');

// Build daemon
if (daemon.build()) {
    console.log('Daemon build successful');
} else {
    console.error('Daemon build failed');
    process.exit(1);
}