const fs = require('fs');
const path = require('path');
const { createFirmwarePackage } = require('./partition.js');

config.set('arch', 'rts3917')

const sdkDir = path.resolve('../sdk/rts39xx_sdk_v5.3/')


config.set('kernelDir', sdkDir+'/out/rts3917n_base/build/linux-custom/');
config.set('libkcapi', sdkDir+'/out/rts3917n_base/build/libkcapi1-1.2.0/')

// Set up RTS3917 cross-compilation toolchain
compiler.setToolchain('rts3917', {
    "chipArch": "arm",
    "path": sdkDir+"/toolchain/asdk-12.4.1-a7-EL-6.6-u1.0-a32nh-linux-x86_64-250925/bin",
    "prefix": "asdk-linux-",
    "host": "arm-linux-uclibcgnueabi",
    "flags": [
      "-fdiagnostics-color=always",
    ]
});

// Command line argument parsing
function parseArgs() {
    const args = config.get('cli.args') || [];

    const result = {
        wifi: '8188fu',      // default wifi chip
        mainChip: 'rts3917n' // default main chip
    };

    for (let arg of args) {
        if (arg.startsWith('--wifi=')) {
            result.wifi = arg.split('=')[1];
        } else if (arg.startsWith('--main_chip=')) {
            result.mainChip = arg.split('=')[1];
        } else if (arg.startsWith('chip=')) {
            result.mainChip = arg.split('=')[1];
        }
    }

    return result;
}

// Get configuration from command line arguments
const buildConfig = parseArgs();
console.log(`Building for chip: ${buildConfig.mainChip}, wifi: ${buildConfig.wifi}`);

// Version and timestamp
function generateVersion() {
    const baseVersion = "01.00.002";
    const now = new Date();
    const timestamp = now.getFullYear().toString() +
                      (now.getMonth() + 1).toString().padStart(2, '0') +
                      now.getDate().toString().padStart(2, '0') +
                      now.getHours().toString().padStart(2, '0') +
                      now.getMinutes().toString().padStart(2, '0') +
                      now.getSeconds().toString().padStart(2, '0');
    return baseVersion + "-" + timestamp;
}

const version = generateVersion();
const chipName = buildConfig.mainChip;
const useEncrypt = false;
const cloud = 'instaview'

config.set('cloud', cloud);
config.set('platform','rts3917');
config.set('ipc_version', version);


// Create temporary directory path
const tempAppDir = `/dev/shm/${chipName}_ipc/app`;
const stagingDir = `staging`;

// Create void target with dependencies on Scons directories
// Dependencies will automatically execute build.js in each directory when created
const buildTarget = createTarget("firmware-package",
"../../../cam/",
"../../../third/hostap/"
);

// Set as virtual target (void/phony type)
buildTarget.setTargetType("void");

// Build function - main build logic
function buildFirmwarePackage() {
    console.log("=== Building Instaview Firmware Package ===");
    console.log(`Version: ${version}`);
    console.log(`Chip: ${chipName}`);
    console.log(`WiFi: ${buildConfig.wifi}`);
    console.log(`Encrypt: ${useEncrypt}`);

    // Prepare temporary directory
    console.log(`Preparing temporary directory: ${tempAppDir}`);

    // Clean and create temp directory
    if (!shell.run(`rm -rf ${tempAppDir}`)) {
        throw new Error(`Failed to remove temp directory: ${tempAppDir}`);
    }

    if (!shell.run(`mkdir -p ${tempAppDir}/bin ${tempAppDir}/lib ${tempAppDir}/rt/rtsnn`)) {
        throw new Error(`Failed to create temp directory: ${tempAppDir}`);
    }

    // Function to copy modules and sensors to staging directory
    function copyModulesAndSensorsToStaging() {
        console.log("Copying modules and sensors to staging directory...");

        // Clean and create staging directory
        if (!shell.run(`rm -rf ${stagingDir}`)) {
            throw new Error(`Failed to remove staging directory: ${stagingDir}`);
        }

        if (!shell.run(`mkdir -p ${stagingDir}/rt/lib/modules/ ${stagingDir}/rt/rtsisp/sensors/`)) {
            throw new Error(`Failed to create staging directory: ${stagingDir}`);
        }

        // Copy modules files to staging directory
        console.log("Copying modules files to staging directory...");
        if (!shell.run(`cp ${sdkDir}/out/rts3917n_base/target/lib/modules/* ${stagingDir}/rt/lib/modules/ --no-dereference -r -v`)) {
            throw new Error("Failed to copy modules files to staging directory");
        }

        // Copy CMOS sensor files to staging directory
        console.log("Copying CMOS sensor files to staging directory...");
        const cmosSensorFiles = [
            "soi/jxf38p/libsensor_jxf38p_mipi.so",
            "soi/jxk306p/libsensor_jxk306p_mipi.so",
            "soi/jxq03p/libsensor_jxq03p_mipi.so",
            "soi/jxk06/libsensor_jxk06.so",
            "soi/jxk347p/libsensor_jxk347p_mipi.so",
            "smartsens/sc2336p/libsensor_sc2336p_mipi.so",
            // Add more sensor file names as needed
        ];

        // Copy each sensor file to staging directory
        const sensorSourceDir = `${sdkDir}/out/rts3917n_base/build/cmos_sensor`;
        for (const sensorFile of cmosSensorFiles) {
            const sourcePath = `${sensorSourceDir}/${sensorFile}`;
            const destPath = `${stagingDir}/rt/rtsisp/sensors/`;

            console.log(`Copying sensor to staging: ${sensorFile}`);
            if (!shell.run(`cp ${sourcePath} ${destPath} --no-dereference -r -v`)) {
                console.warn(`Warning: Failed to copy sensor file to staging: ${sensorFile}`);
            }
        }

        console.log("Modules and sensors copied to staging directory successfully");
    }

    // Function to copy modules and sensors from staging to tempAppDir
    function copyStagingModulesAndSensorsToTempAppDir() {
        console.log("Copying modules and sensors from staging to tempAppDir...");

        // Copy both modules and sensors from staging to tempAppDir in one command
        if (!shell.run(`cp ${stagingDir}/rt/* ${tempAppDir}/rt/ --no-dereference -r -v`)) {
            throw new Error("Failed to copy modules and sensors from staging to tempAppDir");
        }

        console.log("Modules and sensors copied from staging to tempAppDir successfully");
    }
    
    // Use installAllTargets to install build results of all dependency targets
    console.log("Installing all targets to temporary directory...");
    if (!buildTarget.installAllTargets(tempAppDir)) {
        throw new Error("Failed to install targets to temporary directory");
    }

    // Copy app files (using shell.run because this is a local file operation)
    console.log("Copying app files...");
    if (!shell.run(`cp app/* ${tempAppDir} --no-dereference -r`)) {
        throw new Error("Failed to copy app files");
    }

    // Copy modules and sensors to staging directory first, then to tempAppDir
    copyModulesAndSensorsToStaging();

    // Copy modules and sensors from staging to tempAppDir
    copyStagingModulesAndSensorsToTempAppDir();

    // Remove mac80211.ko file from tempAppDir/rt/ using find -exec
    console.log("Removing mac80211.ko file from tempAppDir/rt/...");
    if (!shell.run(`find ${tempAppDir}/rt/ -name "mac80211.ko" -exec rm {} \\;`)) {
        console.warn("Warning: Failed to remove mac80211.ko file (file may not exist)");
    }

    console.log("Removing bluetooth.ko file from tempAppDir/rt/...");
    if (!shell.run(`find ${tempAppDir}/rt/ -name "bluetooth.ko" -exec rm {} \\;`)) {
        console.warn("Warning: Failed to remove bluetooth.ko file (file may not exist)");
    }

    // Copy AI plugin (local file, use shell.run)
    console.log("Copying AI plugin...");
    if (!shell.run(`cp ../AI/rtsnn_odtiny_v1.1_2022.10.07_mnn/plugin/* ${tempAppDir}/rt/rtsnn/ -uv`)) {
        throw new Error("Failed to copy AI plugin");
    }

    // Create squashfs
    console.log("Creating squashfs image...");
    if (!shell.run(`mksquashfs ${tempAppDir} images/app.bin -b 64K -comp xz -fstime 1640995200 -all-root -noappend`)) {
        throw new Error("Failed to create squashfs");
    }

    // Generate fstab.user
    console.log("Generating fstab.user...");
    const encryptOption = useEncrypt ? "encrypt=/etc/keys/crypto_key.bin," : "";
    const fstabContent = `#<type>\t<src>\t<mnt_point>\t<fstype>\t<mnt_flags and options>\t<fsmgr_flags>
nor\t/dev/mtdblock5\t/app\tsquashfs\tro\twait,${encryptOption}verify=/etc/keys/verity_key2.der
nor\t/dev/mtdblock6\t/conf\tjffs2\trw\tdefaults
`;

    if (!shell.run(`mkdir -p rootfs/rootfs_ipcrt/etc`)) {
        throw new Error("Failed to create etc directory");
    }

    fs.writeFileSync("rootfs/rootfs_ipcrt/etc/fstab.user", fstabContent);

    // Build rootfs
    console.log("Building rootfs...");
    if (!shell.run("./mkrootfs.sh")) {
        throw new Error("Failed to build rootfs");
    }

    // Encrypt squashfs
    console.log("Encrypting squashfs...", useEncrypt);
    const encryptSuffix = useEncrypt ? "_encrypt" : "";
    if (!shell.run(`./ipc_encrypt_squashfs.sh ${chipName} ${encryptSuffix}`)) {
        throw new Error("Failed to encrypt squashfs");
    }

    // Create firmware package
    console.log("Creating firmware package...");
    createFirmwarePackageWrapper();

    console.log(`=== Firmware package build completed for ${chipName}! ===`);
}

function createFirmwarePackageWrapper() {
    // Create package with the selected partition table
    const outputDir = `out/${chipName}_${cloud}_sign_${version}`;
    createFirmwarePackage(chipName, version, outputDir);
}

// Execute the build (dependencies will automatically execute builds for each directory)
buildFirmwarePackage();
