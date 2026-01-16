#!/usr/bin/env python3
"""
Simple host-side tool to send firmware to the bootloader simulator via stdin/stdout
This is a helper for local testing when running the simulator binary.
"""
import sys
import time

def send_all(data):
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()

def main():
    if len(sys.argv) < 2:
        print("Usage: host_flash.py <firmware.bin>")
        return
    fn = sys.argv[1]
    with open(fn, 'rb') as f:
        fw = f.read()

    # Simple textual protocol for the simulator
    send_all(b'START')
    time.sleep(0.05)
    # SIZE:len
    send_all(f"SIZE:{len(fw)}".encode())
    time.sleep(0.05)
    # send firmware in chunks
    import math
    CHUNK = 256
    for i in range(0, len(fw), CHUNK):
        send_all(fw[i:i+CHUNK])
        time.sleep(0.01)
    send_all(b'END')
    time.sleep(0.05)
    # compute simple checksum matching bootloader algorithm: sort then adjacent xor
    arr = bytearray(fw)
    arr.sort()
    chk = 0
    for i in range(0, len(arr), 2):
        a = arr[i]
        b = arr[i+1] if i+1 < len(arr) else 0
        chk ^= a ^ b
    send_all(f"CHKSUM:{chk}".encode())

if __name__ == '__main__':
    main()
