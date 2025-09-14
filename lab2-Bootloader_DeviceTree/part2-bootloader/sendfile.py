import struct
import sys
import argparse
import serial
import time

FILENAME = "./lab2_part1_kernel/kernel8.img"
BAUD_RATE = 115200

def calculate_checksum(data):
    """計算 checksum (所有 byte 相加)"""
    return sum(data) & 0xFFFFFFFF  # 取 32-bit 數值

def send_kernel(serial_port):
    """透過 serial 發送 kernel binary 到樹莓派 bootloader"""
    try:
        with open(FILENAME, "rb") as f:
            kernel_data = f.read()
    except FileNotFoundError:
        print(f"Error: Cannot find {FILENAME}")
        sys.exit(1)

    # 計算 checksum
    checksum = calculate_checksum(kernel_data)

    # 構造 boot header
    header = struct.pack('<III', 
        0x544F4F42,  # "BOOT" magic number
        len(kernel_data),  # kernel binary 長度
        checksum  # checksum
    )

    print(f"Sending kernel (size: {len(kernel_data)} bytes, checksum: {checksum:#010x}) to {serial_port}...")
    
    try:
        # 設定 serial 連線
        with serial.Serial(serial_port, BAUD_RATE, timeout=1) as ser:
            ser.write(header)  # 發送 header
            time.sleep(3)
            ser.write(kernel_data)  # 發送 kernel binary
            print("Send complete!")
    except serial.SerialException as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    # 解析終端機參數
    parser = argparse.ArgumentParser(description="Send kernel to Raspberry Pi bootloader via serial port.")
    parser.add_argument("serial_port", help="The serial port to send the kernel to (e.g., /dev/ttyUSB0).")
    
    args = parser.parse_args()
    send_kernel(args.serial_port)
