/*
 * Linux Kernel Driver for 433MHz RF Signal Transmission
 * Complete port of m433.c functionality using hrtimer
 *
 * This driver implements the exact same timing protocol as the original m433.c:
 * - Frame header: 550μs high + 15000μs low
 * - Logic 1: 1500μs high + 550μs low
 * - Logic 0: 550μs high + 1500μs low
 * - 24-bit data frame (20-bit ID + 4-bit reserved)
 * - 20 repeat transmissions for reliability
 */

#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/uaccess.h>


#include "m433_ioctl.h"

#define DRIVER_NAME "m433"
#define DEVICE_NAME "m433"

// Timing parameters (in nanoseconds for hrtimer)
#define TIMING_HEAD_HIGH 550000    // 550μs
#define TIMING_HEAD_LOW 15000000   // 15000μs (15ms)
#define TIMING_LOGIC1_HIGH 1500000 // 1500μs
#define TIMING_LOGIC1_LOW 550000   // 550μs
#define TIMING_LOGIC0_HIGH 550000  // 550μs
#define TIMING_LOGIC0_LOW 1500000  // 1500μs

#define DATA_FRAME_SIZE 24 // 24 bits total
#define ID_BITS_SIZE 20    // First 20 bits are ID
#define REPEAT_COUNT 20    // Send 20 times for reliability

// GPIO timing abstraction
struct gpio_timing_step {
    bool gpio_level;    // GPIO output level (0=low, 1=high)
    u32 duration_ns;    // Duration in nanoseconds
};

// Static timing sequence - calculated once per MAC address change
#define MAX_TIMING_STEPS (2 + (DATA_FRAME_SIZE * 2)) * REPEAT_COUNT  // 1000 steps max
struct gpio_timing_sequence {
    struct gpio_timing_step steps[MAX_TIMING_STEPS];  // Static array
    int total_steps;                                   // Actual number of steps used
    int current_step;                                 // Current step index
};

// Device context structure
struct m433_dev {
    struct miscdevice misc;
    int               gpio_pin;
    bool              gpio_initialized;

    // Timer and timing sequence management
    struct hrtimer timer;
    struct gpio_timing_sequence timing_seq;

    // Data buffer (24-bit frame)
    u8 data_frame[DATA_FRAME_SIZE];

    // Transmission control
    bool transmission_active;
    bool first_trigger;

    // MAC address derived ID
    u8   mac_addr[ETH_ALEN];
    bool mac_valid;

    // User-provided MAC address
    bool user_mac_set;
};

// Global device instance
static struct m433_dev* m433_device;

/* Forward declarations */
static enum hrtimer_restart m433_timer_callback(struct hrtimer* timer);
static void                 m433_generate_id_from_mac(struct m433_dev* dev);
static void                 m433_set_gpio_output(int pin, int value);
static int                  m433_hex_string_to_mac(const char* hex_str, u8* mac_addr);

/* GPIO timing sequence functions */
static int                  m433_generate_data_timing_sequence(struct m433_dev* dev);

/*
 * Add a timing step to the sequence (inline for performance)
 */
static inline void m433_add_timing_step(struct gpio_timing_sequence* seq, int* step_idx, bool level, u32 duration_ns)
{
    seq->steps[*step_idx].gpio_level = level;
    seq->steps[*step_idx].duration_ns = duration_ns;
    (*step_idx)++;
}

/*
 * Generate complete timing sequence from data frame (static array version)
 */
static int m433_generate_data_timing_sequence(struct m433_dev* dev)
{
    struct gpio_timing_sequence* seq = &dev->timing_seq;
    int steps_per_frame = 2 + (DATA_FRAME_SIZE * 2); // Header (2) + Data bits (24*2)
    int total_steps = steps_per_frame * REPEAT_COUNT;
    int step_idx = 0, i, j;

    // Generate timing sequence for each repetition
    for (i = 0; i < REPEAT_COUNT; i++) {
        // Add header: high then low
        m433_add_timing_step(seq, &step_idx, 1, TIMING_HEAD_HIGH);
        m433_add_timing_step(seq, &step_idx, 0, TIMING_HEAD_LOW);

        // Add data bits
        for (j = 0; j < DATA_FRAME_SIZE; j++) {
            if (dev->data_frame[j]) {
                // Logic 1: 1500μs high + 550μs low
                m433_add_timing_step(seq, &step_idx, 1, TIMING_LOGIC1_HIGH);
                m433_add_timing_step(seq, &step_idx, 0, TIMING_LOGIC1_LOW);
            } else {
                // Logic 0: 550μs high + 1500μs low
                m433_add_timing_step(seq, &step_idx, 1, TIMING_LOGIC0_HIGH);
                m433_add_timing_step(seq, &step_idx, 0, TIMING_LOGIC0_LOW);
            }
        }
    }

    // Set total steps and reset for execution
    seq->total_steps = total_steps;
    seq->current_step = 0;

    printk(KERN_INFO "m433: Generated timing sequence with %d steps (%d repetitions)\n",
           total_steps, REPEAT_COUNT);

    return 0;
}

/*
 * Simplified timer callback using GPIO timing sequence
 */
static enum hrtimer_restart m433_timer_callback(struct hrtimer* timer)
{
    struct m433_dev* dev = container_of(timer, struct m433_dev, timer);
    struct gpio_timing_sequence* seq = &dev->timing_seq;
    ktime_t next_interval;

    // Check if transmission is active
    if (!dev->transmission_active) {
        return HRTIMER_NORESTART;
    }

    // Execute current timing step
    if (seq->current_step < seq->total_steps) {
        // Set GPIO level for current step
        m433_set_gpio_output(dev->gpio_pin, seq->steps[seq->current_step].gpio_level);

        // Schedule next step
        next_interval = ktime_set(0, seq->steps[seq->current_step].duration_ns);
        seq->current_step++;

        // Only restart if we have more steps and duration > 0
        if (seq->current_step < seq->total_steps && ktime_to_ns(next_interval) > 0) {
            hrtimer_forward_now(timer, next_interval);
            return HRTIMER_RESTART;
        } else if (seq->current_step < seq->total_steps) {
            // Immediate next step (0 duration)
            return HRTIMER_RESTART;
        }
    }

    // Sequence complete
    dev->transmission_active = false;
    seq->current_step = 0;
    m433_set_gpio_output(dev->gpio_pin, 0);

    printk(KERN_INFO "m433: Timing sequence completed - %d steps executed\n", seq->total_steps);
    return HRTIMER_NORESTART;
}

/*
 * Convert hex string to MAC address
 * Format: "112233445566" -> 6 bytes
 * Returns: 0 on success, negative error code on failure
 */
static int m433_hex_string_to_mac(const char* hex_str, u8* mac_addr)
{
    int i;

    // Validate MAC string format (12 hex characters)
    for (i = 0; i < 12; i++) {
        if (!isxdigit(hex_str[i])) {
            printk(KERN_ERR "m433: Invalid MAC format - not hex digits\n");
            return -EINVAL;
        }
    }

    // Parse hex string to bytes
    for (i = 0; i < ETH_ALEN; i++) {
        int hi = hex_str[i * 2];
        int lo = hex_str[i * 2 + 1];

        // Convert ASCII hex to value
        if (hi >= '0' && hi <= '9')
            hi -= '0';
        else if (hi >= 'a' && hi <= 'f')
            hi -= 'a' - 10;
        else if (hi >= 'A' && hi <= 'F')
            hi -= 'A' - 10;

        if (lo >= '0' && lo <= '9')
            lo -= '0';
        else if (lo >= 'a' && lo <= 'f')
            lo -= 'a' - 10;
        else if (lo >= 'A' && lo <= 'F')
            lo -= 'A' - 10;

        mac_addr[i] = (hi << 4) | lo;
    }

    // Validate MAC address (not multicast, not zero)
    if ((mac_addr[0] & 0x01)
        || (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 && mac_addr[3] == 0 && mac_addr[4] == 0
            && mac_addr[5] == 0)) {
        printk(KERN_ERR "m433: Invalid MAC address - multicast or zero\n");
        return -EINVAL;
    }

    return 0;
}

/*
 * Generate ID from MAC address (equivalent to _make_code())
 * Uses the same algorithm: bits 0-19 from MAC bytes 5,4,3
 */
static void m433_generate_id_from_mac(struct m433_dev* dev)
{
    int idx, mac_idx, mac_bit;

    // Check if user has set MAC address
    if (!dev->user_mac_set) {
        printk(KERN_ERR "m433: MAC address not set by user\n");
        return;
    }

    // Generate 20-bit ID from MAC address (same algorithm as original)
    for (idx = 0; idx < ID_BITS_SIZE; idx++) {
        mac_idx              = 5 - idx / 8; // Use MAC bytes 5,4,3
        mac_bit              = idx % 8;
        dev->data_frame[idx] = (dev->mac_addr[mac_idx] >> mac_bit) & 1;
    }

    // Set remaining 4 bits to 0 (reserved)
    for (idx = ID_BITS_SIZE; idx < DATA_FRAME_SIZE; idx++) {
        dev->data_frame[idx] = 0;
    }

    printk(KERN_INFO "m433: Generated %d-bit ID from user MAC %pM\n", ID_BITS_SIZE, dev->mac_addr);
}

/*
 * Initialize GPIO pin
 */
static int m433_init_gpio(struct m433_dev* dev)
{
    int ret;

    if (dev->gpio_pin < 0) {
        printk(KERN_ERR "m433: GPIO pin not configured\n");
        return M433_ERR_GPIO_INVALID;
    }

    ret = gpio_request(dev->gpio_pin, DRIVER_NAME);
    if (ret) {
        printk(KERN_ERR "m433: Failed to request GPIO %d\n", dev->gpio_pin);
        return ret;
    }

    ret = gpio_direction_output(dev->gpio_pin, 0);
    if (ret) {
        printk(KERN_ERR "m433: Failed to set GPIO direction\n");
        gpio_free(dev->gpio_pin);
        return ret;
    }

    dev->gpio_initialized = true;
    printk(KERN_INFO "m433: GPIO %d initialized successfully\n", dev->gpio_pin);
    return 0;
}

/*
 * Set GPIO output value
 */
static void m433_set_gpio_output(int pin, int value)
{
    if (pin >= 0) {
        gpio_set_value(pin, value ? 1 : 0);
    }
}

/*
 * Start transmission using timing sequence approach
 */
static int m433_start_transmission(struct m433_dev* dev)
{
    int ret;

    // Check if GPIO is initialized
    if (!dev->gpio_initialized) {
        printk(KERN_ERR "m433: GPIO not initialized, cannot start transmission\n");
        return M433_ERR_GPIO_NOT_INIT;
    }

    // Check if transmission is already active
    if (dev->transmission_active) {
        return 0;
    }

    // Generate ID on first trigger (same as original logic)
    if (dev->first_trigger) {
        if (!dev->user_mac_set) {
            printk(KERN_ERR "m433: Cannot start transmission - MAC address not set\n");
            return M433_ERR_GPIO_NOT_INIT;
        }
        m433_generate_id_from_mac(dev);
        dev->first_trigger = false;
    }

    // Generate timing sequence from data frame
    ret = m433_generate_data_timing_sequence(dev);
    if (ret) {
        printk(KERN_ERR "m433: Failed to generate timing sequence: %d\n", ret);
        return ret;
    }

    // Initialize transmission state
    dev->transmission_active = true;

    // Start timer with first timing step (GPIO will be set in timer callback)
    hrtimer_start(&dev->timer, ktime_set(0, 0), HRTIMER_MODE_REL);

    printk(KERN_INFO "m433: Started timing sequence transmission\n");
    return 0;
}

/*
 * Misc device operations
 */
static int m433_open(struct inode* inode, struct file* filp)
{
    struct m433_dev* dev = container_of(filp->private_data, struct m433_dev, misc);
    filp->private_data   = dev;
    return 0;
}

static int m433_release(struct inode* inode, struct file* filp)
{
    return 0;
}

static long m433_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    struct m433_dev* dev = filp->private_data;

    switch (cmd) {
        case M433_IOCTL_TRIGGER_TX:
            return m433_start_transmission(dev);

        case M433_IOCTL_GET_STATUS:
            return dev->transmission_active ? 1 : 0;

        case M433_IOCTL_SET_GPIO:
            if (arg >= 0 && arg <= M433_GPIO_MAX) {
                // If GPIO was previously initialized, clean it up
                if (dev->gpio_initialized) {
                    gpio_set_value(dev->gpio_pin, 0);
                    gpio_free(dev->gpio_pin);
                    dev->gpio_initialized = false;
                }

                dev->gpio_pin = (int)arg;

                // Initialize the new GPIO pin
                if (dev->gpio_pin >= 0) {
                    int ret = m433_init_gpio(dev);
                    if (ret) {
                        dev->gpio_pin = M433_GPIO_DISABLE;
                        return ret;
                    }
                }

                printk(KERN_INFO "m433: GPIO pin set to %d and initialized\n", dev->gpio_pin);
                return 0;
            }
            return -EINVAL;

        case M433_IOCTL_GET_GPIO_STATUS:
            return dev->gpio_initialized ? 1 : 0;

        case M433_IOCTL_SET_MAC: {
            char mac_str[13];
            u8   temp_mac[ETH_ALEN];
            int  ret;

            // Copy MAC string from user space
            if (copy_from_user(mac_str, (char __user*)arg, 12)) {
                return -EFAULT;
            }
            mac_str[12] = '\0';

            // Parse hex string to MAC address
            ret = m433_hex_string_to_mac(mac_str, temp_mac);
            if (ret != 0) {
                return ret;
            }

            // Set the MAC address
            memcpy(dev->mac_addr, temp_mac, ETH_ALEN);
            dev->user_mac_set  = true;
            dev->first_trigger = true; // Reset to regenerate ID on next transmission

            printk(KERN_INFO "m433: MAC address set to %pM\n", dev->mac_addr);
            return 0;
        }

        default:
            return -ENOTTY;
    }
}

static const struct file_operations m433_fops = {
    .owner          = THIS_MODULE,
    .open           = m433_open,
    .release        = m433_release,
    .unlocked_ioctl = m433_ioctl,
};

static struct miscdevice m433_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &m433_fops,
};

/*
 * Module initialization
 */
static int __init m433_init(void)
{
    int ret;

    // Allocate device structure
    m433_device = kzalloc(sizeof(struct m433_dev), GFP_KERNEL);
    if (!m433_device) {
        printk(KERN_ERR "m433: Failed to allocate device memory\n");
        return -ENOMEM;
    }

    // Initialize device structure
    m433_device->gpio_pin            = M433_GPIO_DISABLE; // Not configured
    m433_device->gpio_initialized    = false;
    m433_device->transmission_active = false;
    m433_device->first_trigger       = true;
    m433_device->mac_valid           = false;
    m433_device->user_mac_set        = false;

    // Timing sequence will be initialized when needed

    // Initialize high-resolution timer
    hrtimer_init(&m433_device->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    m433_device->timer.function = m433_timer_callback;

    // Register misc device
    m433_device->misc = m433_misc_device;
    ret               = misc_register(&m433_device->misc);
    if (ret) {
        printk(KERN_ERR "m433: Failed to register misc device\n");
        kfree(m433_device);
        return ret;
    }

    printk(KERN_INFO "m433: 433MHz transmitter driver loaded (timing sequence version)\n");
    printk(KERN_INFO "m433: GPIO not configured, use IOCTL to set GPIO pin\n");
    printk(KERN_INFO "m433: Device node: /dev/%s\n", DEVICE_NAME);

    return 0;
}

/*
 * Module cleanup
 */
static void __exit m433_exit(void)
{
    if (!m433_device)
        return;

    // Stop any active transmission
    if (m433_device->transmission_active) {
        hrtimer_cancel(&m433_device->timer);
        m433_device->transmission_active = false;
    }

    // Cleanup GPIO if initialized
    if (m433_device->gpio_initialized) {
        gpio_set_value(m433_device->gpio_pin, 0);
        gpio_free(m433_device->gpio_pin);
    }

    // Cleanup misc device
    misc_deregister(&m433_device->misc);

    // Free memory
    kfree(m433_device);

    printk(KERN_INFO "m433: Driver unloaded (timing sequence version)\n");
}

module_init(m433_init);
module_exit(m433_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("433MHz Driver Port - Abstracted Version");
MODULE_DESCRIPTION("Linux kernel driver for 433MHz RF signal transmission - GPIO timing sequence abstraction");
MODULE_VERSION("2.0");