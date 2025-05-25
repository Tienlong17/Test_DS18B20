# Test_DS18B20
temperature DS18B20
import glob, time

# Thư mục chứa các device 1-Wire
base_dir = "/sys/bus/w1/devices/"
device_folders = glob.glob(base_dir + "28*")
device_files = [d + "/w1_slave" for d in device_folders]

def read_temp_raw(device_file):
    with open(device_file, 'r') as f:
        return f.readlines()

def read_temp(device_file):
    lines = read_temp_raw(device_file)
    # chờ đến khi CRC ok ("YES")
    while lines[0].strip()[-3:] != "YES":
        time.sleep(0.05)
        lines = read_temp_raw(device_file)
    # phân tích giá trị sau "t="
    temp_str = lines[1].split("t=")[1]
    return float(temp_str) / 1000.0

try:
    while True:
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        for df in device_files:
            temp = read_temp(df)
            print(f"{ts} | {df.split('/')[-2]} = {temp:.2f}°C")
        time.sleep(1)
except KeyboardInterrupt:
    pass
