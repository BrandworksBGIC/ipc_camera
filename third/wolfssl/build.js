const baseDir = 'src';

git.cloneTag('https://github.com/wolfSSL/wolfssl', 'v5.8.4-stable', baseDir)


const path = require('path');
const fs = require('fs');

// Create wolfssl NetLink library target
const wolfssl = createTarget('wolfssl');

// Get current architecture
const curArch = config.get('arch') || 'arm';
const installDir = path.resolve('.', 'install');

// Check configuration file
let perArch = '';
const configFile = '.config';
if (fs.existsSync(configFile)) {
    perArch = fs.readFileSync(configFile, 'utf8').trim();
}

// Build configuration command
const configureArgs = `cd ${baseDir}; ./autogen.sh; ./configure --prefix=${installDir} --host=${config.get('compiler.host') || 'arm-linux-gnueabi'} --enable-shared=no --enable-static=yes --enable-md4 --enable-md5 --enable-cmac --enable-aes --enable-aeskeywrap `;

// If architecture changes, clean and reconfigure
if (perArch !== curArch) {
    
    if (perArch.length > 0) {
        console.log('Architecture changed, cleaning previous build');
        const cleanResult = shell.run(`cd ${baseDir}; make distclean`);
        if (!cleanResult) {
            console.warn('Clean failed:', cleanResult.stderr);
        }
    }
    
    // Execute configuration
    console.log('Configuring wolfssl library');
    const configResult = shell.run(configureArgs);
    if (!configResult) {
        console.error('Configuration failed:', configResult.stderr);
        process.exit(1);
    }

    // Build and install
    console.log('Building and installing wolfssl library');
    const buildResult = shell.run(`cd ${baseDir}; make && make install`);
    if (!buildResult) {
        console.error('Build failed:', buildResult.stderr);
        process.exit(1);
    }
    
    fs.writeFileSync(configFile, curArch, 'utf8');
} else {
    console.log('Configuration unchanged, skipping reconfiguration');
}

// Set target type (wolfssl is built via autotools, marked as complete here)
wolfssl.setTargetType('void');
wolfssl.addIncludeDirs('install/include', true)
wolfssl.addLdfiles('install/lib/libwolfssl*.a')

console.log('wolfssl library build completed');