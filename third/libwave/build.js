git.cloneTag('https://github.com/spock2300/libwave', '1.0.2', "src")


const path = require('path');
const fs = require('fs');

// Create WAV audio library target
const wav = createTarget('wav');

// Get base directory
const baseDir = 'src';

// Apply patches (if patch directory exists)
const patchDir = 'patch';
if (fs.existsSync(patchDir)) {
    const patchFiles = fs.readdirSync(patchDir).filter(file => file.endsWith('.patch'));

    for (const patch of patchFiles) {
        const patchPath = path.join(patchDir, patch);
        git.patch(patchPath, baseDir)
    }
}

// Add source files
wav.addFiles(baseDir + '/src/*.c');

// Add compilation flags
wav.addCflags('-DWAV_ENDIAN_LITTLE', '-Wno-multichar', '-Wno-pointer-to-int-cast');

// Set target type as static library
wav.setTargetType('static');

// Add include directories (public export)
wav.addIncludeDirs(baseDir + '/include', true);

// Build WAV library
if (wav.build()) {
    console.log('WAV audio library build successful');
} else {
    console.error('WAV audio library build failed');
    process.exit(1);
}