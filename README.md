#!/usr/bin/env python3
from w1thermsensor import W1ThermSensor, Unit
import time

# Khởi tạo: tự động phát hiện tất cả DS18B20 trên bus GPIO10
sensors = W1ThermSensor.get_available_sensors()

print(f"Found {len(sensors)} sensor(s):")
for sensor in sensors:
    print("  •", sensor.id)

try:
    while True:
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        for sensor in sensors:
            temp_c = sensor.get_temperature(Unit.DEGREES_C)
            print(f"{timestamp} | {sensor.id} = {temp_c:.2f} °C")
        time.sleep(1)  # đọc mỗi giây
except KeyboardInterrupt:
    print("Stopped by user")
