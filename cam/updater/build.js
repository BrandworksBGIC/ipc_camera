// Create system updater target
const updater = createTarget('updater', '../../third/ed25519');

// Add source files
updater.addFiles('linux/*.c');
updater.addFiles('*.c');

// Set target type as executable
updater.setTargetType('executable');

// Add include directories
updater.addIncludeDirs('.', 'port_include');

// Build system updater
if (updater.build()) {
    console.log('System updater build successful');
} else {
    console.error('System updater build failed');
    process.exit(1);
}