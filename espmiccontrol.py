import time
import serial
import serial.tools.list_ports
import comtypes
from ctypes import cast, POINTER
from comtypes import CLSCTX_ALL
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume

# ---------------- CRC8 ----------------
def crc8(data: str) -> int:
    c = 0
    for ch in data:
        c ^= ord(ch)
    return c

def frame(type_, payload):
    body = f"{type_}|{payload}"
    c = crc8(body)
    return f"^{body}|0x{c:02X}$"

# ---------------- MIC CONTROL ----------------
def get_mic():
    devices = AudioUtilities.GetMicrophone()
    interface = devices.Activate(
        IAudioEndpointVolume._iid_, CLSCTX_ALL, None
    )
    return cast(interface, POINTER(IAudioEndpointVolume))

def is_muted():
    return bool(get_mic().GetMute())

def mute():
    get_mic().SetMute(1, None)

def unmute():
    get_mic().SetMute(0, None)

def toggle():
    vol = get_mic()
    vol.SetMute(0 if vol.GetMute() else 1, None)

# ---------------- AUTO PORT DETECTION ----------------
def find_esp32_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if (p.vid, p.pid) in [
            (0x303A, 0x1001),
            (0x303A, 0x1002),
            (0x303A, 0x1003),
            (0x10C4, 0xEA60),
            (0x1A86, 0x7523),
        ]:
            return p.device
    return None

# ---------------- MAIN ----------------
def main():
    port = find_esp32_port()
    if not port:
        print("ESP32 not found")
        return

    print("Using port:", port)
    ser = serial.Serial(port, 115200, timeout=0.1)

    last_heartbeat = time.time()

    while True:
        # Send mic status
        status = "MUTED" if is_muted() else "UNMUTED"
        ser.write(frame("S", status).encode())

        # Heartbeat
        if time.time() - last_heartbeat > 3:
            ser.write(frame("H", "PING").encode())
            last_heartbeat = time.time()

        # Read incoming commands
        if ser.in_waiting:
            data = ser.read_until(b"$").decode(errors="ignore")
            print("RX:", data)   # <‑‑ add this
            if "TOGGLE" in data:
                toggle()

            elif "UNMUTE" in data:
                unmute()

            elif "MUTE" in data:
                mute()

        time.sleep(0.1)

if __name__ == "__main__":
    main()
