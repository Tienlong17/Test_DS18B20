import RPi.GPIO as GPIO
import time

# Chân 1-Wire (DQ) nối vào GPIO4 (BCM)
PIN = 4

# ROM + Function commands
SKIP_ROM        = 0xCC
CONVERT_T       = 0x44
READ_SCRATCHPAD = 0xBE

def usleep(micro):
    """Sleep micro giây"""
    time.sleep(micro / 1000000.0)

def init_gpio():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(PIN, GPIO.OUT, initial=GPIO.HIGH)

def deinit_gpio():
    GPIO.cleanup()

def reset_pulse():
    """
    Gửi reset pulse, sau đó poll presence trong ~300µs
    Trả về True nếu nhận được presence pulse
    """
    # Master kéo bus xuống 0 ≥480µs
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    usleep(500)
    # Release bus
    GPIO.output(PIN, GPIO.HIGH)
    # Chuyển sang input để đọc presence
    GPIO.setup(PIN, GPIO.IN)
    # Poll presence trong 300 µs
    presence = False
    start = time.time()
    while (time.time() - start) < 0.0003:
        if GPIO.input(PIN) == 0:
            presence = True            
            break
    # Hoàn tất slot bằng cách chờ thêm ~200 µs
    if presence == True:
        print('da xong reset_pulse()')
    usleep(200)
    return presence

def write_bit(bit):
    """ Ghi 1 bit lên bus theo timing 1-Wire """
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    if bit:
        usleep(10)    # giữ ~10µs
        GPIO.output(PIN, GPIO.HIGH)
        usleep(55)
    else:
        usleep(65)    # giữ ~65µs
        GPIO.output(PIN, GPIO.HIGH)
        usleep(5)

def read_bit():
    """ Đọc 1 bit từ bus theo timing 1-Wire """
    GPIO.setup(PIN, GPIO.OUT)
    GPIO.output(PIN, GPIO.LOW)
    usleep(3)
    GPIO.setup(PIN, GPIO.IN)
    usleep(10)
    bit = GPIO.input(PIN)
    usleep(53)
    return bit

def write_byte(byte):
    """ Gửi một byte (LSB trước) """
    for i in range(8):
        write_bit((byte >> i) & 1)
    usleep(5)

def read_byte():
    """ Đọc một byte (LSB trước) """
    val = 0
    for i in range(8):
        if read_bit():
            val |= 1 << i
    return val

def read_temperature():
    # --- Bước 1: Reset & Presence
    if not reset_pulse():
        raise RuntimeError("Không nhận được presence pulse lần 1!")

    # --- Bước 2: Skip ROM + Convert T
    write_byte(SKIP_ROM)
    write_byte(CONVERT_T)
    print('da gui convert T')
    # Chờ tối đa 750 ms cho conversion độ phân giải 12-bit
    for _ in range(1000):
        if read_bit():
            break
        usleep(100)
    else:
        raise RuntimerError('time out converting')
 

    # --- Bước 4: Skip ROM + Read Scratchpad
    write_byte(SKIP_ROM)
    write_byte(READ_SCRATCHPAD)

    # --- Bước 5: Đọc 9 byte scratchpad
    scratch = [read_byte() for _ in range(9)]
    print("Scratchpad data:", [hex(b) for b in scratch])

    lsb, msb = scratch[0], scratch[1]
    raw = (msb << 8) | lsb
    if raw > 0x7FFF:
        raw -= 1 <<16

    temp_c = raw / 16.0
    return temp_c

if __name__ == "__main__":
    try:
        init_gpio()
        while True:
            temp = read_temperature()
            print(f"Nhiệt độ: {temp:.4f} °C\n")
            time.sleep(1)
    except KeyboardInterrupt:
        print("Kết thúc đo.")
    finally:
        deinit_gpio()

