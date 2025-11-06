const path = require('path')
// Create RTL8188FU WiFi driver target
const rtl8188 = createTarget('rtl8188');
rtl8188.setTargetType('void')

// Get base directory

const baseDir = path.resolve('.','../../../chip/rts3917/sdk/rts39xx_sdk_v5.3/platform/source/drivers/wifi/rtl8188ftv/');

// Get configuration
const kernelDir = config.get('kernelDir');
const chipArch = config.get('compiler.chipArch');
const hostPrefix = config.get('compiler.prefix');

// Execute make command to build WiFi driver kernel module
const makeResult = shell.run(`cd ${baseDir}; make ARCH=${chipArch} KSRC=${kernelDir} CROSS_COMPILE=${hostPrefix} INSTALL_MOD_STRIP=1 CONFIG_RTW_MINIMAL_MEMORY_USAGE=y`);

if (makeResult) {
    console.log('RTL8188FU WiFi driver build successful');
} else {
    console.error('RTL8188FU WiFi driver build failed:', makeResult);
    process.exit(1);
}

rtl8188.addInstallFiles("drivers", `${baseDir}/8188fu.ko`)