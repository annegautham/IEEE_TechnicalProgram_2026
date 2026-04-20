import time

def send_i2c_command(ser, opCode, params=None):
    if params is None:
        params = []
    
    num_params = len(params)
    
    # 1. Send the 'W' command character
    ser.write(b'W')
    
    # 2. Wait for Teensy ACK (0xAC)
    ack = ser.read(1)
    if ack != b'\xAC':
        print(f"Error: Teensy did not ACK the W command. Received: {ack}")
        return

    # 3. Send OpCode and Number of Parameters
    # Using bytes([val1, val2]) converts them to raw bytes
    ser.write(bytes([opCode, num_params]))

    # 4. Send Parameters one by one (as WRITING_PARAMETERS expects)
    if num_params > 0:
        for p in params:
            ser.write(bytes([p]))
            # Optional: Short delay if the Teensy prints too much text between bytes
            time.sleep(0.01)

    # 5. Wait for final ACK (0xAA)
    final_ack = ser.read(1)
    if final_ack == b'\xAA':
        print(f"✅ Command 0x{opCode:02X} executed successfully.")
    else:
        print(f"⚠️ Command sent, but no final ACK. Received: {final_ack}")