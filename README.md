#!/usr/bin/env python3
import glob
import time
import os

# Thư mục sysfs của 1-Wire
BASE_DIR = "/sys/bus/w1/devices/"
# Tất cả device DS18B20 bắt đầu bằng "28-"
device_folders = glob.glob(os.path.join(BASE_DIR, "28*"))
device_files = [os.path.join(folder, "w1_slave") for folder in device_folders]

def read_raw(device_file):
    """Đọc 2 dòng thô từ w1_slave."""
    with open(device_file, "r") as f:
        return f.readlines()

def read_temp(device_file):
    """
    Chờ đến khi CRC OK ("YES"), rồi parse giá trị sau 't='
    Trả về nhiệt độ (°C, float).
    """
    lines = read_raw(device_file)
    # Nếu CRC chưa OK, chờ và đọc lại
    while lines[0].strip()[-3:] != "YES":
        time.sleep(0.1)
        lines = read_raw(device_file)
    # Lấy phần t=xxxxx
    temp_str = lines[1].split("t=")[-1]
    return float(temp_str) / 1000.0

def main(poll_interval=1.0):
    if not device_files:
        print("Không tìm thấy DS18B20 nào trên bus 1-Wire.")
        return

    print(f"Tìm thấy {len(device_files)} sensor:")
    for df in device_files:
        print("  •", os.path.basename(os.path.dirname(df)))

    try:
        while True:
            timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
            for df in device_files:
                sensor_id = os.path.basename(os.path.dirname(df))
                temp_c = read_temp(df)
                print(f"{timestamp} | {sensor_id} = {temp_c:.3f} °C")
            time.sleep(poll_interval)
    except KeyboardInterrupt:
        print("\nDừng chương trình.")

if __name__ == "__main__":
    main()
