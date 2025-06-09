#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define GPIO_PIN "4" // GPIO4 (BCM numbering)
#define GPIO_PATH "/sys/class/gpio"
#define DELAY_US(x) usleep(x) // microseconds

void gpio_export(const char *pin) {
    char path[64];
    int fd = open(GPIO_PATH "/export", O_WRONLY);
    if (fd < 0) return;
    write(fd, pin, strlen(pin));
    close(fd);
}

void gpio_unexport(const char *pin) {
    char path[64];
    int fd = open(GPIO_PATH "/unexport", O_WRONLY);
    if (fd < 0) return;
    write(fd, pin, strlen(pin));
    close(fd);
}

void gpio_direction(const char *pin, const char *dir) {
    char path[64];
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, dir, strlen(dir));
    close(fd);
}

void gpio_write(const char *pin, int value) {
    char path[64], val = value ? '1' : '0';
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, &val, 1);
    close(fd);
}

int gpio_read(const char *pin) {
    char path[64], val;
    snprintf(path, sizeof(path), GPIO_PATH "/gpio%s/value", pin);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    read(fd, &val, 1);
    close(fd);
    return val == '1';
}

void delay_us(unsigned int us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

// 1-Wire reset, trả về 1 nếu DS18B20 có mặt, 0 nếu không
int onewire_reset(const char *pin) {
    gpio_direction(pin, "out");
    gpio_write(pin, 0);
    delay_us(480);
    gpio_direction(pin, "in");
    delay_us(70);
    int present = !gpio_read(pin);
    delay_us(410);
    return present;
}

void onewire_write_bit(const char *pin, int bit) {
    gpio_direction(pin, "out");
    gpio_write(pin, 0);
    if (bit) {
        delay_us(6);
        gpio_direction(pin, "in");
        delay_us(64);
    } else {
        delay_us(60);
        gpio_direction(pin, "in");
        delay_us(10);
    }
}

int onewire_read_bit(const char *pin) {
    int bit;
    gpio_direction(pin, "out");
    gpio_write(pin, 0);
    delay_us(6);
    gpio_direction(pin, "in");
    delay_us(9);
    bit = gpio_read(pin);
    delay_us(55);
    return bit;
}

void onewire_write_byte(const char *pin, unsigned char byte) {
    for (int i = 0; i < 8; ++i)
        onewire_write_bit(pin, (byte >> i) & 1);
}

unsigned char onewire_read_byte(const char *pin) {
    unsigned char byte = 0;
    for (int i = 0; i < 8; ++i)
        if (onewire_read_bit(pin))
            byte |= (1 << i);
    return byte;
}

float ds18b20_read_temp(const char *pin) {
    unsigned char scratchpad[9];
    int temp_raw;

    // 1. Reset & Skip ROM & Convert T
    if (!onewire_reset(pin)) {
        printf("Sensor not detected.\n");
        return -999;
    }
    onewire_write_byte(pin, 0xCC); // Skip ROM
    onewire_write_byte(pin, 0x44); // Convert T
    sleep(1); // Chờ chuyển đổi (max 750ms)

    // 2. Reset & Skip ROM & Read Scratchpad
    if (!onewire_reset(pin)) {
        printf("Sensor not detected.\n");
        return -999;
    }
    onewire_write_byte(pin, 0xCC); // Skip ROM
    onewire_write_byte(pin, 0xBE); // Read Scratchpad

    for (int i = 0; i < 9; ++i)
        scratchpad[i] = onewire_read_byte(pin);

    temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    // Xử lý số âm
    if (temp_raw & 0x8000) temp_raw = (temp_raw ^ 0xFFFF) + 1, temp_raw = -temp_raw;
    return temp_raw / 16.0;
}

int main() {
    gpio_export(GPIO_PIN);
    usleep(100000); // Đợi cho Pi export xong
    printf("Đang đọc nhiệt độ...\n");
    for (int i = 0; i < 10; ++i) {
        float temp = ds18b20_read_temp(GPIO_PIN);
        if (temp > -100)
            printf("Nhiet do: %.2f C\n", temp);
        else
            printf("Khong tim thay cam bien!\n");
        sleep(1);
    }
    gpio_unexport(GPIO_PIN);
    return 0;
}
