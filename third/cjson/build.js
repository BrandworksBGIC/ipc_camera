git.cloneTag('https://github.com/DaveGamble/cJSON.git', 'v1.7.18', "src")

// Create cJSON static library target
const cJSON = createTarget('cJSON');

// Add source files
cJSON.addFiles('src/cJSON.c');

// Set target type as static library
cJSON.setTargetType('static');

// Add include directories (public export)
cJSON.addIncludeDirs('src', true);

// Build cJSON library
if (cJSON.build()) {
    console.log('cJSON library build successful');
} else {
    console.error('cJSON library build failed');
    process.exit(1);
}