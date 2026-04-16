import serial
from PIL import Image
import numpy as np
import time
import cleanImages
import miscFuncs

def sendImage(IMAGE_PATH, COM_PORT, BAUD, TIMEOUT, CHUNK):
    # =========================
    # LOAD + PREP IMAGE
    # =========================
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
    crc = miscFuncs.crc16(data)
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

