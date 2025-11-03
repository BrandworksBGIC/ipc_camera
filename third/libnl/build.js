git.clone('https://github.com/thom311/libnl.git', "src")

const path = require('path');
const fs = require('fs');

// Create libnl NetLink library target
const libnl = createTarget('libnl');

// Get base directory
const baseDir = 'src';

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
const configureArgs = `cd ${baseDir}; ./autogen.sh; ./configure --prefix=${installDir} --host=${config.get('compiler.host') || 'arm-linux-gnueabi'} --enable-shared=no --enable-static=yes`;

// If architecture changes, clean and reconfigure
if (perArch !== curArch) {
    fs.writeFileSync(configFile, curArch, 'utf8');

    if (perArch.length > 0) {
        console.log('Architecture changed, cleaning previous build');
        const cleanResult = shell.run(`cd ${baseDir}; make distclean`);
        if (!cleanResult) {
            console.warn('Clean failed:', cleanResult.stderr);
        }
    }

    // Execute configuration
    console.log('Configuring libnl library');
    const configResult = shell.run(configureArgs);
    if (!configResult) {
        console.error('Configuration failed:', configResult.stderr);
        process.exit(1);
    }

    // Build and install
    console.log('Building and installing libnl library');
    const buildResult = shell.run(`cd ${baseDir}; make && make install`);
    if (!buildResult) {
        console.error('Build failed:', buildResult.stderr);
        process.exit(1);
    }
} else {
    console.log('Configuration unchanged, skipping reconfiguration');
}

// Set target type (libnl is built via autotools, marked as complete here)
libnl.setTargetType('static');
libnl.addIncludeDirs('install/include', true)
libnl.addLdfiles('install/lib/libnl*.a')

console.log('libnl library build completed');