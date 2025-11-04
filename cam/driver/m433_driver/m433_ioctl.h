/*
 * M433 433MHz RF Transmitter IOCTL Interface Definitions
 *
 * This header defines the IOCTL commands for the M433 kernel driver
 * following Linux kernel conventions for IOCTL command encoding.
 */

#ifndef _M433_IOCTL_H
#define _M433_IOCTL_H

#include <linux/ioctl.h>

/* Magic number for M433 device identification */
#define M433_IOC_MAGIC 'M'

/* IOCTL command definitions using Linux kernel standard encoding */

/**
 * M433_IOCTL_TRIGGER_TX - Trigger 433MHz transmission
 *
 * Starts the transmission of a 433MHz RF signal using the configured
 * GPIO pin and timing protocol. Requires GPIO to be initialized first.
 *
 * Arguments: None
 * Returns: 0 on success, negative error code on failure
 */
#define M433_IOCTL_TRIGGER_TX _IO(M433_IOC_MAGIC, 0)

/**
 * M433_IOCTL_GET_STATUS - Get transmission status
 *
 * Returns the current transmission status of the device.
 *
 * Arguments: None
 * Returns: 1 if transmission is active, 0 if idle
 */
#define M433_IOCTL_GET_STATUS _IO(M433_IOC_MAGIC, 1)

/**
 * M433_IOCTL_SET_GPIO - Set GPIO pin and initialize
 *
 * Sets the GPIO pin for RF signal transmission and initializes it.
 * If a GPIO was previously configured, it will be cleaned up first.
 *
 * Arguments: GPIO pin number (int, 0-255 for valid GPIO, negative to disable)
 * Returns: 0 on success, negative error code on failure
 */
#define M433_IOCTL_SET_GPIO _IOW(M433_IOC_MAGIC, 2, int)

/**
 * M433_IOCTL_GET_GPIO_STATUS - Get GPIO initialization status
 *
 * Returns the current GPIO initialization status.
 *
 * Arguments: None
 * Returns: 1 if GPIO is initialized and ready, 0 if not configured
 */
#define M433_IOCTL_GET_GPIO_STATUS _IO(M433_IOC_MAGIC, 3)

/**
 * M433_IOCTL_SET_MAC - Set MAC address for ID generation
 *
 * Sets the MAC address used for generating the 20-bit ID.
 * MAC address format: 12-character hex string (e.g., "112233445566")
 *
 * Arguments: Pointer to 13-character buffer for MAC string (including null terminator)
 * Returns: 0 on success, negative error code on failure
 */
#define M433_IOCTL_SET_MAC _IOW(M433_IOC_MAGIC, 4, char[13])

/* Error codes specific to M433 driver */
#define M433_ERR_GPIO_NOT_INIT -ENODEV /* GPIO not initialized */
#define M433_ERR_GPIO_INVALID -EINVAL  /* Invalid GPIO pin number */
#define M433_ERR_GPIO_BUSY -EBUSY      /* GPIO pin already in use */
#define M433_ERR_TRANS_ACTIVE -EAGAIN  /* Transmission already active */

/* Maximum GPIO pin number supported */
#define M433_GPIO_MAX 255

/* Special GPIO values */
#define M433_GPIO_DISABLE -1

/* Version information */
#define M433_IOCTL_VERSION 1
#define M433_IOCTL_VERSION_STR "1.0"

#endif /* _M433_IOCTL_H */