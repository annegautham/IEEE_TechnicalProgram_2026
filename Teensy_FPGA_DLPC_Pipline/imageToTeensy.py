import serial
from PIL import Image
import numpy as np
import time
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, filedialog

# =========================
# CONFIG
# =========================
ports = [port.device for port in serial.tools.list_ports.comports()]

selected_port = None
selected_image = None

def browse_file():
    global selected_image
    file_path = filedialog.askopenfilename(
        title="Select Image",
        filetypes=[
            ("Image Files", "*.png *.jpg *.jpeg *.bmp *.tiff"),
            ("All Files", "*.*")
        ]
    )
    if file_path:
        selected_image = file_path
        file_label.config(text=file_path)

def start():
    global selected_port
    selected_port = combo.get()
    root.destroy()

# =========================
# UI
# =========================
root = tk.Tk()
root.title("Setup")

tk.Label(root, text="Choose COM Port:").pack(pady=5)

combo = ttk.Combobox(root, values=ports)
combo.pack(pady=5)

# Image picker
tk.Button(root, text="Select Image", command=browse_file).pack(pady=5)

file_label = tk.Label(root, text="No file selected", wraplength=300)
file_label.pack(pady=5)

tk.Button(root, text="Start", command=start).pack(pady=10)

root.mainloop()

# =========================
# RESULTS
# =========================
print("Selected COM:", selected_port)
print("Selected Image:", selected_image)

COM_PORT = selected_port
BAUD = 115200
TIMEOUT = 1       

CHUNK = 512            # must match Teensy buffer
IMAGE_PATH = selected_image

# =========================
# CRC16 (TI SPEC)
# =========================
def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x8005) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

# =========================
# LOAD + PREP IMAGE
# =========================
img = Image.open(IMAGE_PATH).convert("L")  # grayscale
arr = np.array(img, dtype=np.uint8)

height, width = arr.shape

# needs to be 1280 × 720 for FPGA
if (width, height) != (1280, 720):
    if input("Warning: Image is not 1280x720. Resize? (y/n) ").lower() == 'y':
        img = img.resize((1280, 720))
        arr = np.array(img, dtype=np.uint8)
        height, width = arr.shape
    else:
        print("Exiting.")
        exit()

print(f"Image: {width}x{height}")

# Flatten to 1D byte stream
data = arr.flatten().tobytes()

print(f"Total bytes: {len(data)}")

# =========================
# SERIAL CONNECT
# =========================
ser = serial.Serial(COM_PORT, BAUD, timeout=TIMEOUT)
time.sleep(2)  # wait for Teensy reset

print("Sending...")

# =========================
# SEND DATA IN CHUNKS
# =========================
sent = 0

while sent < len(data):
    chunk = data[sent:sent+CHUNK]
    ser.write(chunk)

    ack = ser.read(1)
    if ack != b'\xAA':
        print("Error: No ACK received")
        break

    sent += len(chunk)

    # Just progress update every 20 chunks
    if sent % (CHUNK * 20) == 0:
        print(f"Sent {sent}/{len(data)} bytes")

print("Done sending image")

# =========================
# SEND CRC (OPTIONAL DEBUG)
# =========================
crc = crc16(data)
print(f"CRC16: {hex(crc)}")

# Send CRC as little endian
ser.reset_input_buffer()
ser.write(bytes([crc & 0xFF, (crc >> 8) & 0xFF]))

results = ser.read(2)

if len(results) < 2:
    print("Incomplete response from Teensy")
else:
    resultTeensy, resultFPGA = results[0:1], results[1:2]

    if resultTeensy == b'\xCC':
        print("✅ CRC MATCH TEENSY")
    elif resultTeensy == b'\xEE':
        print("❌ CRC MISMATCH TEENSY")
    else:
        print("⚠️ Unknown Teensy response")

    if resultFPGA == b'\xDD':
        print("✅ CRC MATCH FPGA")
    elif resultFPGA == b'\xEF':
        print("❌ CRC MISMATCH FPGA")
    else:
        print("⚠️ Unknown FPGA response")

ser.close()