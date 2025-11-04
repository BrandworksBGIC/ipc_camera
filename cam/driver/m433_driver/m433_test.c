/*
 * Test program for 433MHz Linux Kernel Driver
 * Tests the functionality of the m433 kernel driver
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


#define DEVICE_PATH "/dev/m433"

// Include IOCTL command definitions from shared header
#include "m433_ioctl.h"

void print_usage(const char* prog_name)
{
    printf("Usage: %s [command] [options]\n", prog_name);
    printf("Commands:\n");
    printf("  trigger              - Trigger 433MHz transmission\n");
    printf("  status               - Get transmission status\n");
    printf("  set_gpio <pin>      - Set GPIO pin and initialize (0-%d)\n", M433_GPIO_MAX);
    printf("  gpio_status          - Get GPIO initialization status\n");
    printf("  set_mac <address>   - Set MAC address for ID generation (e.g., 112233445566)\n");
    printf("  get_mac              - Get current MAC address\n");
    printf("  test                 - Run comprehensive test\n");
    printf("  monitor              - Monitor status continuously\n");
    printf("  help                 - Show this help\n");
}

int trigger_transmission(int fd)
{
    printf("Triggering 433MHz transmission...\n");

    // Use IOCTL to trigger transmission
    if (ioctl(fd, M433_IOCTL_TRIGGER_TX) < 0) {
        perror("IOCTL trigger failed");
        return -1;
    }

    printf("Transmission triggered successfully\n");
    return 0;
}

int get_status(int fd)
{
    // Use IOCTL to get status
    int status = ioctl(fd, M433_IOCTL_GET_STATUS);
    if (status < 0) {
        perror("IOCTL status failed");
        return -1;
    }

    printf("Transmission status: %s\n", status ? "ACTIVE" : "IDLE");

    return status;
}

int set_gpio_pin(int fd, int pin)
{
    if (pin < 0 || pin > M433_GPIO_MAX) {
        printf("Invalid GPIO pin: %d (must be 0-%d)\n", pin, M433_GPIO_MAX);
        return -1;
    }

    if (ioctl(fd, M433_IOCTL_SET_GPIO, pin) < 0) {
        perror("Set GPIO failed");
        return -1;
    }

    printf("GPIO pin set to %d and initialized\n", pin);
    return 0;
}

int get_gpio_status(int fd)
{
    int status = ioctl(fd, M433_IOCTL_GET_GPIO_STATUS);
    if (status < 0) {
        perror("Get GPIO status failed");
        return -1;
    }

    printf("GPIO initialization status: %s\n", status ? "READY" : "NOT CONFIGURED");
    return status;
}

int validate_mac_address(const char* mac_str)
{
    int len = strlen(mac_str);

    // Check length (should be 12 characters for MAC address)
    if (len != 12) {
        printf("Invalid MAC address length: %d (should be 12 characters)\n", len);
        return -1;
    }

    // Check all characters are valid hex digits
    for (int i = 0; i < len; i++) {
        char c = mac_str[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f'))) {
            printf("Invalid character '%c' at position %d (must be 0-9, A-F, a-f)\n", c, i);
            return -1;
        }
    }

    return 0;
}

int set_mac_address(int fd, const char* mac_str)
{
    if (validate_mac_address(mac_str) < 0) {
        printf("MAC address validation failed\n");
        return -1;
    }

    // Prepare MAC address buffer (13 chars: 12 for MAC + 1 for null terminator)
    char mac_buffer[13];
    strncpy(mac_buffer, mac_str, 12);
    mac_buffer[12] = '\0';  // Ensure null termination

    if (ioctl(fd, M433_IOCTL_SET_MAC, mac_buffer) < 0) {
        perror("Set MAC address failed");
        return -1;
    }

    printf("MAC address set to: %.2s:%.2s:%.2s:%.2s:%.2s:%.2s\n",
           mac_buffer, mac_buffer+2, mac_buffer+4,
           mac_buffer+6, mac_buffer+8, mac_buffer+10);
    return 0;
}

int get_mac_address(int fd)
{
    // Note: Getting MAC address would require a corresponding GET_MAC ioctl command
    // For now, we'll display a message indicating the current status
    printf("MAC address status: Not directly readable via current IOCTL interface\n");
    printf("To verify MAC address, check driver logs or implement M433_IOCTL_GET_MAC\n");
    return 0;
}

void run_comprehensive_test(int fd)
{
    printf("Running comprehensive test...\n\n");

    // Test 1: Check initial GPIO status
    printf("Test 1: Initial GPIO status\n");
    get_gpio_status(fd);
    printf("\n");

    // Test 2: Set MAC address
    printf("Test 2: Set MAC address\n");
    if (set_mac_address(fd, "112233445566") == 0) {
        printf("✓ MAC address set successfully\n");
    }
    printf("\n");

    // Test 3: Try transmission without GPIO (should fail)
    printf("Test 3: Try transmission without GPIO (should fail)\n");
    if (trigger_transmission(fd) < 0) {
        printf("✓ Transmission correctly failed - GPIO not configured\n");
    }
    printf("\n");

    // Test 4: Set GPIO pin
    printf("Test 4: Set GPIO pin\n");
    set_gpio_pin(fd, 14); // Set to GPIO 14
    get_gpio_status(fd);
    printf("\n");

    // Test 5: Single transmission
    printf("Test 5: Single transmission\n");
    if (trigger_transmission(fd) == 0) {
        printf("✓ Transmission succeeded\n");
    }
    sleep(1);
    get_status(fd);
    printf("\n");

    // Test 6: Multiple transmissions
    printf("Test 6: Multiple transmissions (3 times)\n");
    for (int i = 0; i < 3; i++) {
        printf("Transmission %d/3...\n", i + 1);
        trigger_transmission(fd);
        usleep(200000); // 200ms between transmissions
    }
    sleep(1);
    get_status(fd);
    printf("\n");

    // Test 7: MAC address change
    printf("Test 7: MAC address change\n");
    if (set_mac_address(fd, "AABBCCDDEEFF") == 0) {
        printf("✓ MAC address changed successfully\n");
    }
    trigger_transmission(fd);
    sleep(1);
    printf("\n");

    // Test 8: GPIO change
    printf("Test 8: GPIO pin change\n");
    set_gpio_pin(fd, 14); // Change to GPIO 14
    get_gpio_status(fd);
    trigger_transmission(fd);
    sleep(1);
    printf("\n");

    // Test 9: Test GPIO disable
    printf("Test 9: Disable GPIO\n");
    set_gpio_pin(fd, -1); // Disable GPIO
    get_gpio_status(fd);
    if (trigger_transmission(fd) < 0) {
        printf("✓ Transmission correctly failed - GPIO disabled\n");
    }
    printf("\n");

    printf("Comprehensive test completed.\n");
}

void monitor_status(int fd, int duration)
{
    printf("Monitoring transmission status for %d seconds...\n", duration);
    printf("Press Ctrl+C to stop monitoring\n\n");

    for (int i = 0; i < duration; i++) {
        printf("Time: %02d:%02d - ", i / 60, i % 60);
        get_status(fd);
        sleep(1);
    }
}

int main(int argc, char* argv[])
{
    int fd;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Open device
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        printf("Make sure the driver is loaded and device node exists:\n");
        printf("  sudo insmod m433_kernel_driver.ko\n");
        printf("  sudo mknod /dev/m433 c <major> 0\n");
        printf("  sudo chmod 666 /dev/m433\n");
        return 1;
    }

    printf("433MHz Test Program\n");
    printf("Device: %s opened successfully\n\n", DEVICE_PATH);

    // Parse command
    if (strcmp(argv[1], "trigger") == 0) {
        trigger_transmission(fd);
    } else if (strcmp(argv[1], "status") == 0) {
        get_status(fd);
    } else if (strcmp(argv[1], "set_gpio") == 0) {
        if (argc < 3) {
            printf("Error: set_gpio requires a pin number\n");
            print_usage(argv[0]);
            close(fd);
            return 1;
        }
        set_gpio_pin(fd, atoi(argv[2]));
    } else if (strcmp(argv[1], "gpio_status") == 0) {
        get_gpio_status(fd);
    } else if (strcmp(argv[1], "set_mac") == 0) {
        if (argc < 3) {
            printf("Error: set_mac requires a MAC address (12 hex characters)\n");
            print_usage(argv[0]);
            close(fd);
            return 1;
        }
        set_mac_address(fd, argv[2]);
    } else if (strcmp(argv[1], "get_mac") == 0) {
        get_mac_address(fd);
    } else if (strcmp(argv[1], "test") == 0) {
        run_comprehensive_test(fd);
    } else if (strcmp(argv[1], "monitor") == 0) {
        int duration = 30; // Default 30 seconds
        if (argc > 2) {
            duration = atoi(argv[2]);
        }
        monitor_status(fd, duration);
    } else if (strcmp(argv[1], "help") == 0) {
        print_usage(argv[0]);
    } else {
        printf("Unknown command: %s\n\n", argv[1]);
        print_usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}