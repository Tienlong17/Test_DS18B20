// File: ds18b20_onwire.c
// Compile: gcc -O2 -o ds18b20_onwire ds18b20_onwire.c -lrt

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define GPIO_BASE      0x3F200000UL
#define BLOCK_SIZE     (4*1024)
#define GPIO_PIN       10   // DQ trên GPIO10

volatile uint32_t *gpio;

// helpers for delays
static void udelay(unsigned us) {
    struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = us * 1000
    };
    nanosleep(&ts, NULL);
}

// set GPIO_PIN mode: 0=input, 1=output
static void gpio_mode(int pin, int is_output) {
    uint32_t reg = pin / 10;
    uint32_t shift = (pin % 10) * 3;
    uint32_t val = gpio[reg];
    val &= ~(7 << shift);
    if (is_output) val |= (1 << shift);
    gpio[reg] = val;
}

// write pin: 0=low, 1=high (hi-Z for 1-wire)
static void gpio_write(int pin, int value) {
    if (value) {
        // set to input (hi-Z) để pull-up kéo lên
        gpio_mode(pin, 0);
    } else {
        // drive low
        gpio_mode(pin, 1);
        gpio[(pin/32)+7] = 1 << (pin%32);  // GPCLR0
    }
}

// read pin
static int gpio_read(int pin) {
    uint32_t lvl = gpio[(pin/32)+13];  // GPLEV0
    return (lvl & (1 << pin)) != 0;
}

// 1-Wire reset + presence
static int ow_reset(void) {
    int present;
    gpio_write(GPIO_PIN, 0);   // pull DQ low
    udelay(480);
    gpio_write(GPIO_PIN, 1);   // release
    udelay(70);
    present = !gpio_read(GPIO_PIN); // presence = low
    udelay(410);
    return present;
}

// write 1 bit
static void ow_write_bit(int bit) {
    gpio_write(GPIO_PIN, 0);
    if (bit) {
        udelay(6);
        gpio_write(GPIO_PIN, 1);
        udelay(64);
    } else {
        udelay(60);
        gpio_write(GPIO_PIN, 1);
        udelay(10);
    }
}

// read 1 bit
static int ow_read_bit(void) {
    int bit;
    gpio_write(GPIO_PIN, 0);
    udelay(6);
    gpio_write(GPIO_PIN, 1);
    udelay(9);
    bit = gpio_read(GPIO_PIN);
    udelay(55);
    return bit;
}

// write one byte, LSB first
static void ow_write_byte(uint8_t b) {
    for (int i = 0; i < 8; i++) {
        ow_write_bit(b & 1);
        b >>= 1;
    }
}

// read one byte, LSB first
static uint8_t ow_read_byte(void) {
    uint8_t b = 0;
    for (int i = 0; i < 8; i++) {
        if (ow_read_bit()) b |= (1 << i);
    }
    return b;
}

// read temperature from DS18B20 (skip ROM)
float ds18b20_read_temp(void) {
    uint8_t temp_lsb, temp_msb;
    if (!ow_reset()) {
        fprintf(stderr, "No device present!\n");
        return NAN;
    }
    ow_write_byte(0xCC);  // Skip ROM
    ow_write_byte(0x44);  // Convert T
    // wait conversion: tối đa 750ms cho 12-bit
    usleep(750000);

    if (!ow_reset()) {
        fprintf(stderr, "No device on read!\n");
        return NAN;
    }
    ow_write_byte(0xCC);
    ow_write_byte(0xBE);  // Read Scratchpad

    temp_lsb = ow_read_byte();
    temp_msb = ow_read_byte();
    // bỏ qua 7 byte còn lại (CRC...)
    for (int i = 0; i < 7; i++) ow_read_byte();

    int16_t raw = (temp_msb << 8) | temp_lsb;
    return raw / 16.0f;   // 12-bit resolution
}

int main() {
    int mem_fd;
    void *gpio_map;

    // mở /dev/gpiomem để mmap
    if ((mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC)) < 0) {
        perror("open /dev/gpiomem");
        return 1;
    }
    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0);
    close(mem_fd);
    if (gpio_map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    gpio = (volatile uint32_t *)gpio_map;

    // main loop
    while (1) {
        float temp = ds18b20_read_temp();
        if (!isnan(temp)) {
            printf("Temperature: %.3f °C\n", temp);
        }
        sleep(1);
    }

    munmap(gpio_map, BLOCK_SIZE);
    return 0;
}
