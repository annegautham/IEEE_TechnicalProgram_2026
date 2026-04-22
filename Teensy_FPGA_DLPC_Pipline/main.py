import serial
import os
import computerSelectors, commandToTeensy, imageToTeensy

# =========================
# CONFIG & HELPERS
# =========================
BAUD = 115200
TIMEOUT = 2
CHUNK_SIZE = 512

# =========================
# INITIAL SETUP (PORT ONLY)
# =========================
port = computerSelectors.get_com_ports()
if not port:
    print("Error: No COM ports detected.")
    exit()

SELECTED_PORT = port # Default to first; or use your UI to pick
print(f"Using Port: {SELECTED_PORT}")

# =========================
# MAIN LOOP
# =========================
try:
    # Initialize Serial once
    ser = serial.Serial(SELECTED_PORT, BAUD, timeout=TIMEOUT)
    print(f"Connected to {SELECTED_PORT} at {BAUD} baud.")

    while True:
        print("\n--- Serial Transmission Menu ---")
        action = input("Send: (I)mage, (F)older, (C)ommand. OR (Q)uit or use new (P)ort: ").lower()

        if action == 'i':
            image_path = computerSelectors.select_file_ui()
            if image_path:
                SEND_OK = imageToTeensy.sendImage(image_path, ser, CHUNK_SIZE)
            else:
                print("Selection cancelled.")

        elif action == 'f':
            folder_path = computerSelectors.select_folder_ui()
            if folder_path:
                print("Starting batch send...")
                imageToTeensy.sendFolderOfImages(folder_path, ser, CHUNK_SIZE)
            else:
                print("Selection cancelled.")

        elif action == 'c':
            try:
                # Example input: 1A 01 02 (OpCode=1A, Params=[01, 02])
                raw_input = input("Enter Command (Hex OpCode) and Params (Hex) [e.g. 1A 00 FF]: ")
                parts = raw_input.split()

                # Convert hex strings to integers
                op_code = int(parts[0], 16)
                params = [int(p, 16) for p in parts[1:]]

                # Call the new function
                commandToTeensy.send_i2c_command(ser, op_code, params)

            except ValueError:
                print("Invalid format. Use Hex values separated by spaces (e.g., 0B 01).")

        elif action == 'q':
            print("Exiting...")
            break

        elif action == 'p':
            port = computerSelectors.get_com_ports()
            if not port:
                print("Error: No COM ports detected.")
                exit()

            SELECTED_PORT = port # Default to first; or use your UI to pick
            print(f"Using Port: {SELECTED_PORT}")
            ser = serial.Serial(SELECTED_PORT, BAUD, timeout=TIMEOUT)
            print(f"Connected to {SELECTED_PORT} at {BAUD} baud.")

        else:
            print("Invalid input. Please choose I, F, C, Q, or P.")

except serial.SerialException as e:
    print(f"Serial Error: {e}")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
        print("Serial connection closed.")