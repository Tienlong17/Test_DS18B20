#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define GPIO_NUM 4
#define GPIO_PATH "/sys/class/gpio"
#define MAX_BUF 64

#define SKIP_ROM        0xCC
#define CONVERT_T       0x44
#define READ_SCRATCHPAD 0xBE

void gpio_export(int gpio) {
    char buffer[MAX_BUF];
    int fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd < 0) return;
    snprintf(buffer, MAX_BUF, "%d", gpio);
    write(fd, buffer, strlen(buffer));
    close(fd);
    usleep(100000);  // chờ sysfs tạo file
}

void gpio_unexport(int gpio) {
    char buffer[MAX_BUF];
    int fd = open(GPIO_PATH "/unexport", O_WRONLY);
    if (fd < 0) return;
    snprintf(buffer, MAX_BUF, "%d", gpio);
    write(fd, buffer, strlen(buffer));
    close(fd);
}

void gpio_direction(int gpio, const char *dir) {
    char path[MAX_BUF];
    snprintf(path, MAX_BUF, GPIO_PATH "/gpio%d/direction", gpio);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, dir, strlen(dir));
    close(fd);
}

void gpio_write(int gpio, int value) {
    char path[MAX_BUF];
    snprintf(path, MAX_BUF, GPIO_PATH "/gpio%d/value", gpio);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    if (value)
        write(fd, "1", 1);
    else
        write(fd, "0", 1);
    close(fd);
}

int gpio_read(int gpio) {
    char path[MAX_BUF];
    char val;
    snprintf(path, MAX_BUF, GPIO_PATH "/gpio%d/value", gpio);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    read(fd, &val, 1);
    close(fd);
    return val == '1';
}

void delay_us(int us) {
    usleep(us);
}

int reset_pulse() {
    gpio_direction(GPIO_NUM, "out");
    gpio_write(GPIO_NUM, 0);
    delay_us(500);

    gpio_direction(GPIO_NUM, "in");
    delay_us(70);

    int presence = !gpio_read(GPIO_NUM);
    delay_us(430);
    return presence;
}

void write_bit(int bit) {
    gpio_direction(GPIO_NUM, "out");
    gpio_write(GPIO_NUM, 0);

    if (bit) {
        delay_us(10);
        gpio_write(GPIO_NUM, 1);
        delay_us(55);
    } else {
        delay_us(65);
        gpio_write(GPIO_NUM, 1);
        delay_us(5);
    }
}

int read_bit() {
    gpio_direction(GPIO_NUM, "out");
    gpio_write(GPIO_NUM, 0);
    delay_us(2);

    gpio_direction(GPIO_NUM, "in");
    delay_us(12);
    int bit = gpio_read(GPIO_NUM);
    delay_us(50);

    return bit;
}

void write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        write_bit((byte >> i) & 1);
    }
    delay_us(5);
}

uint8_t read_byte() {
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        if (read_bit()) {
            val |= (1 << i);
        }
    }
    return val;
}

float read_temperature() {
    if (!reset_pulse()) {
        fprintf(stderr, "No presence 1\n");
        return -999;
    }

    write_byte(SKIP_ROM);
    write_byte(CONVERT_T);

    for (int i = 0; i < 1000; i++) {
        if (read_bit()) break;
        delay_us(1000);
    }

    if (!reset_pulse()) {
        fprintf(stderr, "No presence 2\n");
        return -999;
    }

    write_byte(SKIP_ROM);
    write_byte(READ_SCRATCHPAD);

    uint8_t scratch[9];
    for (int i = 0; i < 9; i++) {
        scratch[i] = read_byte();
    }

    int16_t raw = (scratch[1] << 8) | scratch[0];
    if (raw & 0x8000) raw -= 0x10000;

    return raw / 16.0;
}

int main() {
    gpio_export(GPIO_NUM);

    while (1) {
        float temp = read_temperature();
        printf("Nhiệt độ: %.2f °C\n", temp);
        sleep(1);
    }

    gpio_unexport(GPIO_NUM);
    return 0;
}
