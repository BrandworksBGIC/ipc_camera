git.cloneTag('https://git.w1.fi/hostap.git', 'hostap_2_11', "src")



const path = require('path');
const fs = require('fs');



const hostap = createTarget('hostap', "../libnl");

const baseDir = 'src';
const libnlDir = path.resolve('.', '../libnl');


const curArch = config.get('arch') || 'arm';
const toolchainPrefix = config.get('compiler.host');

// Check configuration file
let perArch = '';
const configFile = '.config';
if (fs.existsSync(configFile)) {
    perArch = fs.readFileSync(configFile, 'utf8').trim();
}

// hostap configuration content
const hostapConfig = `CONFIG_LIBNL32=y
CONFIG_DRIVER_NL80211=y
CONFIG_TLS=openssl
CONFIG_INTERNAL_LIBTOMMATH=y
CONFIG_WEP=n
CONFIG_AP=y
CONFIG_IEEE80211R=y
CONFIG_IEEE80211W=y
CONFIG_NO_CONFIG_WRITE=y
CONFIG_CTRL_IFACE=y
CONFIG_SAE=y
CONFIG_P2P=y

#CONFIG_NO_STDOUT_DEBUG=y
CONFIG_NO_WPA_MSG=y
CONFIG_NO_HOSTAPD_LOGGER=y
`;

// Build command arguments
const buildArgs = `cd ${baseDir}/wpa_supplicant; make all -j 4`;
const sslBaseDir =  path.resolve('../../chip/rts3917/sdk/rts39xx_sdk_v5.3/out/rts3917n_base/build/libopenssl-custom/');


// If architecture changes, clean and reconfigure
if (perArch !== curArch) {
    
    if (perArch.length > 0) {
        console.log('Architecture changed, cleaning previous build');
        const cleanResult = shell.run(`cd ${baseDir}/wpa_supplicant; make distclean`);
        if (!cleanResult) {
            console.warn('Clean failed:', cleanResult.stderr);
        }
    }
    
    // Write wpa_supplicant configuration file
    const wpaConfigPath = path.join(baseDir, 'wpa_supplicant', '.config');
    fs.writeFileSync(wpaConfigPath, hostapConfig, 'utf8');
    
    // Add OpenSSL path
    const additionalConfig = `CC=${toolchainPrefix}-gcc\nCFLAGS += -I${sslBaseDir}/include/\nLIBS += -L${sslBaseDir} -lpthread -L${libnlDir}/install/lib\nLIBNL_INC=${libnlDir}/install/include/libnl3\n`;
    fs.appendFileSync(wpaConfigPath, additionalConfig, 'utf8');

    
    // Execute build
    console.log('Building hostap wpa_supplicant');
    const buildResult = shell.run(buildArgs);
    if (!buildResult) {
        console.error('Build failed:', buildResult.stderr);
        process.exit(1);
    }
    
    
    fs.writeFileSync(configFile, curArch, 'utf8');
} else {
    console.log('Configuration unchanged, skipping rebuild');
}

// Set target type (hostap is built via make, marked as complete here)
hostap.setTargetType('void');
// Create installation directory and copy binary files
const installDir = path.resolve('.', 'install');
fs.mkdirSync(path.join(installDir, 'bin'));

console.log('Installing wpa_supplicant tools');
const tools = ['wpa_supplicant', 'wpa_cli', 'wpa_passphrase'];
for (const tool of tools) {

    hostap.addInstallFiles('bin', `${baseDir}/wpa_supplicant/${tool}`)
}

hostap.addInstallFiles('lib', `${sslBaseDir}/libcrypto.so`)
hostap.addInstallFiles('lib', `${sslBaseDir}/libssl.so`)

console.log('hostap WiFi hotspot library build completed');