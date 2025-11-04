// Create Flash programming tool target
const flashcp = createTarget('flashcp');

// Set target type as executable
flashcp.setTargetType('executable');

// Add source files
flashcp.addFiles('*.c');

// Build Flash programming tool
if (flashcp.build()) {
    console.log('Flash programming tool build successful');
} else {
    console.error('Flash programming tool build failed');
    process.exit(1);
}