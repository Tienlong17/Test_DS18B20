import glob
import time

base_dir = '/sys/bus/w1/devices/'
device_folders = glob.glob(base_dir + '28-*')
if not device_folders:
    raise RuntimeError('Không tìm thấy DS18B20!')

device_file = device_folders[0] + '/w1_slave'

def read_temp():
    with open(device_file, 'r') as f:
        lines = f.readlines()
    if lines[0].strip()[-3:] != 'YES':
        return None
    equals_pos = lines[1].find('t=')
    if equals_pos != -1:
        temp_string = lines[1][equals_pos + 2:]
        temp_c = float(temp_string) / 1000.0
        return temp_c
    return None

while True:
    temp = read_temp()
    if temp is not None:
        print(f"Nhiệt độ: {temp:.3f} °C")
    else:
        print("Chưa đọc được nhiệt độ!")
    time.sleep(1)
