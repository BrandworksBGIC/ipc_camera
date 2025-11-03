git.cloneTag('https://github.com/mchehab/zbar.git', '0.23.93', "src")


const path = require('path');
const fs = require('fs');

// Create ZBar QR code recognition library target
const zbar = createTarget('zbar');

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
const configureArgs = `cd ${baseDir}; autoreconf -vfi && ./configure --prefix=${installDir} --host=${config.get('compiler.host') || ''} --disable-video --without-jpeg --without-imagemagick --without-gtk --without-python --without-qt --enable-shared=no --enable-static=yes --with-java=no --enable-pthread=no --with-x=no --with-dbus=no`;

// If architecture changes, clean and reconfigure
// || config.get("rebuild")
if (perArch !== curArch) {
    
    if (perArch.length > 0) {
        console.log('Architecture changed, cleaning previous build');
        const cleanResult = shell.exec(`cd ${baseDir}; make distclean`);
        if (!cleanResult.success) {
            console.warn('Clean failed:', cleanResult.stderr);
        }
    }
    
    // Execute configuration
    console.log('Configuring ZBar library');
    const configResult = shell.run(configureArgs);
    if (!configResult) {
        console.error('Configuration failed:', configResult);
        process.exit(1);
    }
    
    // Build and install
    console.log('Building and installing ZBar library');
    const buildResult = shell.run(`cd ${baseDir}; make && make install`);
    if (!buildResult) {
        console.error('Build failed:', buildResult);
        process.exit(1);
    }
    fs.writeFileSync(configFile, curArch, 'utf8');
} else {
    console.log('Configuration unchanged, skipping reconfiguration');
}

// Set target type (ZBar is built via autotools, marked as complete here)
zbar.setTargetType('void');

zbar.addIncludeDirs('install/include', true)
zbar.addLdfiles('install/lib/libzbar.a')

console.log('ZBar library build completed');