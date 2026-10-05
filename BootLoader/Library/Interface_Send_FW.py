import serial
import struct
import time
import os
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog

BAUDRATE = 115200  

#cách dùng: python Interface_Send_FW.py

def calc_checksum(data: bytes) -> int:
    return sum(data) & 0xFFFFFFFF

def send_firmware(fw_file, serial_port):
    if not os.path.exists(fw_file):
        messagebox.showerror("Lỗi", f"File '{fw_file}' không tồn tại!")
        return

    ser = serial.Serial(serial_port, BAUDRATE, bytesize=8, parity='N', stopbits=1, timeout=5)
    time.sleep(3)
    ser.reset_input_buffer()
    print(f"[PC] Waiting for BL_READY on {serial_port}...")

    # Chờ MCU báo BL_READY
    for _ in range(5):
        bl_msg = ser.readline().decode(errors="ignore").strip()
        print(f"[MCU] {bl_msg}")
        if "BL_READY" in bl_msg:
            break
    else:
        messagebox.showerror("Lỗi", "Bootloader không phản hồi!")
        ser.close()
        return

    with open(fw_file, "rb") as f:
        fw_data = f.read()

    fw_size = len(fw_data)
    fw_sum = calc_checksum(fw_data)

    print(f"[PC] Firmware size: {fw_size} bytes")
    print(f"[PC] Checksum    : 0x{fw_sum:08X}")

    header = struct.pack("<II", fw_size, fw_sum)
    ser.write(header)
    time.sleep(1)

    ChunkLen = 256
    for i in range(0, fw_size, ChunkLen):
        Chunk = fw_data[i:i+ChunkLen]
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        ser.write(Chunk)
        print(f"[PC] Sent Chunk {i//ChunkLen}, size {len(Chunk)} bytes")
        time.sleep(0.05)

    resp = ser.readline().decode(errors="ignore").strip()
    print(f"[MCU] {resp}")  
    ser.close()
    messagebox.showinfo("Hoàn tất", f"Nạp firmware xong!\nMCU phản hồi: {resp}")

def main():
    root = tk.Tk()
    root.withdraw()  # Ẩn cửa sổ chính

    # Hộp thoại chọn file
    fw_file = filedialog.askopenfilename(
        title="Chọn file firmware (.bin)",
        filetypes=[("BIN files", "*.bin"), ("All files", "*.*")]
    )
    if not fw_file:
        return

    # Hỏi COM port
    serial_port = simpledialog.askstring("Nhập cổng COM", "VD: COM5")
    if not serial_port:
        return

    send_firmware(fw_file, serial_port)

if __name__ == "__main__":
    main()
