// Create motor driver target
const motorDriver = createTarget('driver_motor');
motorDriver.setTargetType('void')


// Get configuration
const kernelDir = config.get('kernelDir');
const hostPrefix = config.get('compiler.prefix');
const chipArch = config.get('compiler.chipArch');
const arch = config.get('arch').toUpperCase();

// Execute make command to build motor driver kernel module
const makeResult = shell.run(`make ARCH=${chipArch} KERNEL_DIR=${kernelDir} CROSS_COMPILE=${hostPrefix} IPC_ARCH=${arch}`);

if (makeResult) {
    console.log('Motor driver build successful');
} else {
    console.error('Motor driver build failed:', makeResult);
    process.exit(1);
}

motorDriver.addInstallFiles("drivers", `ipc_step_motor.ko`)