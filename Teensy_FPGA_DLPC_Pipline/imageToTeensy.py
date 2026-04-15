import serial
from PIL import Image
import numpy as np
import time
import serial.tools.list_ports
import tkinter as tk
from tkinter import ttk, filedialog
import os
import re
import cleanImages

# =========================
# CONFIG
# =========================
ports = [port.device for port in serial.tools.list_ports.comports()]

selected_port = None
selected_image = None
selected_folder = None

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
        image_label.config(text=file_path)

def find_folder():
    global selected_folder
    folder_path = filedialog.askdirectory(title="Select Folder")
    if folder_path:
        selected_folder = folder_path
        file_label.config(text=folder_path)

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

image_label = tk.Label(root, text="No file selected", wraplength=300)
image_label.pack(pady=5)

# Folder w/ image picker
tk.Button(root, text="Select Folder With Images", command=find_folder).pack(pady=5)

file_label = tk.Label(root, text="No file selected", wraplength=300)
file_label.pack(pady=5)

tk.Button(root, text="Start", command=start).pack(pady=10)

root.mainloop()

# =========================
# RESULTS
# =========================
print("Selected COM:", selected_port)
print("Selected Image:", selected_image)
print("Selected Folder:", selected_folder)

COM_PORT = selected_port
BAUD = 115200
TIMEOUT = 2      

CHUNK = 512           
IMAGE_PATH = selected_image
FOLDER_PATH = selected_folder

# files = [f for f in os.listdir(selected_folder) if f.endswith(".png") and "tile_y" in f]

# coords = []
# for f in files:
#     match = re.search(r'tile_y(\d+)_x(\d+)', f)
#     if match:
#         coords.append((int(match.group(2)), int(match.group(1)))) # (x, y)

# nx = max(c[0] for c in coords) + 1
# ny = max(c[1] for c in coords) + 1

# # Sort coords so that it goes left to right and then moves up
# coords = sorted(coords, key=lambda c: (c[1], c[0]))

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
# for ix, iy in coords:
#     IMAGE_PATH = os.path.join(selected_folder, f"tile_y{iy:02d}_x{ix:02d}.png")
cleanImages.remove_black_markers(IMAGE_PATH, IMAGE_PATH)
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
# print(f"Printing: tile_y{iy:02d}_x{ix:02d}.png")

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
        print("Error: Wrong ACK received")
        print('Received: ', ack)
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
ser.write(bytes([crc & 0xFF, (crc >> 8) & 0xFF]))

results = ser.read(3)

# For some reason teensy sends xAA xCC and then xDD and I think it's because
# there's a leftover ACK from last chunk but when I tried to remove that last
# ACK it didn't register an ACK for the last chunk so I made the executive decision
# to just ignore it and slice it here!
results = results[1:3]

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