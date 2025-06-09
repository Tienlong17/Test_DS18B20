import RPi.GPIO as GPIO
import time

# Pin 1-Wire (DQ) nối vào GPIO4 (BCM)
PIN = 4

# Các lệnh ROM + Function
SKIP_ROM        = 0xCC
CONVERT_T       = 0x44
READ_SCRATCHPAD = 0xBE

# Delay helper (micro giây)
def usleep(micro):
    time.sleep(micro / 1_000_000.0)

def init_gpio():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(PIN, GPIO.OUT, initial=GPIO.HIGH)

def deinit_gpio():
    GPIO.cleanup()

def reset_pulse():
    """ Master kéo bus xuống 0 ít nhất 480µs, sau đó lên 1 và chờ presence """
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    usleep(500)              # ≥480µs
    GPIO.output(PIN, GPIO.HIGH)
    usleep(70)               # Chờ presence window
    GPIO.setup(PIN, GPIO.IN) # Đọc presence
    presence = GPIO.input(PIN) == 0
    usleep(410)              # Hoàn tất slot
    return presence

def write_bit(bit):
    """ Ghi 1 bit lên bus """
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    if bit:
        # Ghi “1”: giữ 1–15µs rồi release, rest of slot high
        usleep(10)
        GPIO.output(PIN, GPIO.HIGH)
        usleep(55)
    else:
        # Ghi “0”: giữ 60µs, sau đó release
        usleep(65)
        GPIO.output(PIN, GPIO.HIGH)
        usleep(5)

def read_bit():
    """ Đọc 1 bit từ bus """
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    usleep(3)                 # ≥1µs
    GPIO.setup(PIN, GPIO.IN)  # release bus
    usleep(10)                # chờ DS18B20 trả bit
    bit = GPIO.input(PIN)
    usleep(53)                # hoàn tất slot
    return bit

def write_byte(byte):
    """ Gửi 8 bit, LSB trước """
    for i in range(8):
        write_bit((byte >> i) & 1)
    usleep(5)

def read_byte():
    """ Đọc 8 bit, LSB trước """
    val = 0
    for i in range(8):
        if read_bit():
            val |= 1 << i
    return val

def read_temperature():
    # 1) Reset + Presence
    if not reset_pulse():
        raise RuntimeError("Không nhận được presence pulse!")

    # 2) Skip ROM + Convert T
    write_byte(SKIP_ROM)
    write_byte(CONVERT_T)
    # Chờ tối đa 750ms (12-bit). Có thể poll bus, nhưng đơn giản dùng delay:
    time.sleep(0.75)

    # 3) Reset + Presence
    if not reset_pulse():
        raise RuntimeError("Không nhận được presence pulse lần 2!")

    # 4) Skip ROM + Read Scratchpad
    write_byte(SKIP_ROM)
    write_byte(READ_SCRATCHPAD)

    # 5) Đọc 9 byte
    data = [read_byte() for _ in range(9)]

    # 6) CRC bạn có thể kiểm tra ở đây (byte 8)
    # Bỏ qua CRC check trong ví dụ này

    # 7) Tính nhiệt độ
    raw = (data[1] << 8) | data[0]
    # Chuyển signed
    if raw & 0x8000:
        raw = -((raw ^ 0xFFFF) + 1)
    temp_c = raw / 16.0
    return temp_c

if __name__ == "__main__":
    try:
        init_gpio()
        while True:
            temp = read_temperature()
            print(f"Nhiệt độ hiện tại: {temp:.3f} °C")
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        deinit_gpio()
