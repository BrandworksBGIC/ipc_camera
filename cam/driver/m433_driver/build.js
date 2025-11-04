// Create M433 wireless communication driver target
const m433Driver = createTarget('m433_driver');
m433Driver.setTargetType('void')

// Get base directory

// Get configuration
const kernelDir = config.get('kernelDir');
const hostPrefix = config.get('compiler.prefix');

// Execute make command to build M433 driver kernel module
const makeResult = shell.run(`make KERNEL_DIR=${kernelDir} CROSS_COMPILE=${hostPrefix}`);

if (makeResult) {
    console.log('M433 wireless driver build successful');
} else {
    console.error('M433 wireless driver build failed:', makeResult);
    process.exit(1);
}

m433Driver.addInstallFiles("drivers", `m433_driver.ko`)