// Create GPIO driver target
const gpioDriver = createTarget('gpio_driver');
gpioDriver.setTargetType('void')

// Get base directory

// Get kernel path and host toolchain prefix
const kernelDir = config.get('kernelDir');
const hostPrefix = config.get('compiler.prefix');
const chipArch = config.get('compiler.chipArch');

// Execute make command to build kernel module
const makeResult = shell.run(`make  ARCH=${chipArch} KERNEL_DIR=${kernelDir} CROSS_COMPILE=${hostPrefix}`);

if (makeResult) {
    console.log('GPIO driver build successful');
} else {
    console.error('GPIO driver build failed:', makeResult);
    process.exit(1);
}

gpioDriver.addInstallFiles("drivers", `ipc-gpio.ko`)