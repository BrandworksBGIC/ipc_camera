// Partition table configuration for RTS3917 firmware builds

/**
 * Get partition table for RTS3917N chip with encryption and signing
 * @param {string} chipName - The chip name (rts3917n or rts3918n)
 * @returns {Object} UpdatePackage partitions configuration
 */
function getEncryptSignedTable(chipName) {
    const partitions = UpdatePackage.NewPartitions(256, 64 * 1024);

    partitions.addPartition(`../build_security_image/${chipName}/release/fip.bin`, "uboot", 0, 5, 1);
    partitions.addPartition(``, "bootdtb", 0, 1, 2);
    partitions.addPartition(`../build_security_image/${chipName}/release/linux.crypted.itb`, "kernel", 1, 49, 3);
    partitions.addPartition(`../build_security_image/${chipName}/release/rootfs.squashfs.signed.crypted`, "rootfs", 1, 25, 4);
    partitions.addPartition(`../build_security_image/${chipName}/release/app.bin.signed.crypted`, "app", 2, 170, 5);
    partitions.addPartition("", "config", -1, 6, 6);

    return partitions;
}

function getSignedTable(chipName) {
    const partitions = UpdatePackage.NewPartitions(256, 64 * 1024);

    partitions.addPartition(`../build_security_image/${chipName}/release/fip.bin`, "uboot", 0, 5, 1);
    partitions.addPartition(``, "bootdtb", 0, 1, 2);
    partitions.addPartition(`../build_security_image/${chipName}/release/linux.itb`, "kernel", 1, 49, 3);
    partitions.addPartition(`../build_security_image/${chipName}/release/rootfs.squashfs.signed`, "rootfs", 1, 25, 4);
    partitions.addPartition(`../build_security_image/${chipName}/release/app.bin.signed`, "app", 2, 170, 5);
    partitions.addPartition("", "config", -1, 6, 6);

    return partitions;
}



/**
 * Create firmware package with the specified chip configuration
 * @param {string} chipName - The chip name
 * @param {string} version - The firmware version
 * @param {string} outputDir - Output directory for the package
 */
function createFirmwarePackage(chipName, version, outputDir) {
    const partitions = getSignedTable(chipName);

    // Create package with the selected partition table
    const pkg = partitions.newPackage(chipName, outputDir, "echo", "cp_upgrade_key", "cppa");

    pkg.flash(version);
    pkg.ota();

    console.log(`Firmware package created: ${outputDir}`);
    return pkg;
}

// Export functions for use in build.js
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        getEncryptSignedTable,
        createFirmwarePackage
    };
}