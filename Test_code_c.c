#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wiringPi.h>
#include <unistd.h>

#define DQ_PIN 7  // wiringPi pin 7 = BCM GPIO4

#define SKIP_ROM        0xCC
#define CONVERT_T       0x44
#define READ_SCRATCHPAD 0xBE

void usleep_safe(unsigned int microseconds) {
    usleep(microseconds);
}

void pin_output() {
    pinMode(DQ_PIN, OUTPUT);
}

void pin_input() {
    pinMode(DQ_PIN, INPUT);
}

void write_low() {
    digitalWrite(DQ_PIN, LOW);
}

void write_high() {
    digitalWrite(DQ_PIN, HIGH);
}

int reset_pulse() {
    pin_output();
    write_low();
    usleep_safe(500); // ≥480µs

    write_high();
    pin_input();
    usleep_safe(70); // đợi presence

    int presence = digitalRead(DQ_PIN) == 0;
    usleep_safe(430); // còn lại ~500µs

    return presence;
}

void write_bit(int bit) {
    pin_output();
    write_low();

    if (bit) {
        usleep_safe(10);
        write_high();
        usleep_safe(55);
    } else {
        usleep_safe(65);
        write_high();
        usleep_safe(5);
    }
}

int read_bit() {
    pin_output();
    write_low();
    usleep_safe(2);

    pin_input();
    usleep_safe(12);
    int bit = digitalRead(DQ_PIN);
    usleep_safe(50);

    return bit;
}

void write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        write_bit((byte >> i) & 1);
    }
    usleep_safe(5);
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
        fprintf(stderr, "Không có presence lần 1!\n");
        return -999;
    }

    write_byte(SKIP_ROM);
    write_byte(CONVERT_T);

    // Đợi convert xong (~750ms)
    for (int i = 0; i < 1000; i++) {
        if (read_bit()) break;
        usleep_safe(1000); // 1ms
    }

    if (!reset_pulse()) {
        fprintf(stderr, "Không có presence lần 2!\n");
        return -999;
    }

    write_byte(SKIP_ROM);
    write_byte(READ_SCRATCHPAD);

    uint8_t scratch[9];
    for (int i = 0; i < 9; i++) {
        scratch[i] = read_byte();
    }

    int16_t raw = (scratch[1] << 8) | scratch[0];
    if (raw & 0x8000) raw = raw - 0x10000;

    return raw / 16.0;
}

int main() {
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "Không khởi tạo được wiringPi\n");
        return 1;
    }

    while (1) {
        float temp = read_temperature();
        printf("Nhiệt độ: %.4f °C\n", temp);
        sleep(1);
    }

    return 0;
}
