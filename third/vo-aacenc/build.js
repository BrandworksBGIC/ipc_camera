git.cloneTag('https://github.com/mstorsjo/vo-aacenc', "v0.1.3", "src")

const path = require('path');
const fs = require('fs');

// Create ipc_aacenc target
const ipc_aacenc = createTarget('ipc_aacenc');

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
const configureArgs = `cd ${baseDir}; autoreconf -vfi && ./configure --prefix=${installDir} --host=${config.get('compiler.host') || ''} `;

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
    console.log('Configuring vo_aacenc library');
    const configResult = shell.run(configureArgs);
    if (!configResult) {
        console.error('Configuration failed:', configResult.stderr);
        process.exit(1);
    }

    // Build and install
    console.log('Building and installing vo_aacenc library');
    const buildResult = shell.run(`cd ${baseDir}; make && make install`);
    if (!buildResult) {
        console.error('Build failed:', buildResult.stderr);
        process.exit(1);
    }
} else {
    console.log('Configuration unchanged, skipping reconfiguration');
}


ipc_aacenc.setTargetType('static');
ipc_aacenc.addIncludeDirs('include', true)
ipc_aacenc.addIncludeDirs('install/include')

ipc_aacenc.addLdfiles('install/lib/libvo-aacenc.a')
ipc_aacenc.addFiles('aac.c')
ipc_aacenc.build()

console.log('ipc_aacenc library build completed');