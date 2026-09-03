#!/usr/bin/env python3
"""Read a 16-bit register from a TC358746-style I2C device using raw i2c-dev ioctl.
Usage: i2c_rd16.py <bus> <addr_hex> <reg_hex> [count]
"""
import sys
import os
import fcntl
import ctypes
import struct

I2C_RDWR = 0x0707
I2C_M_RD = 0x0001


class i2c_msg(ctypes.Structure):
    _fields_ = [
        ("addr", ctypes.c_uint16),
        ("flags", ctypes.c_uint16),
        ("len", ctypes.c_uint16),
        ("buf", ctypes.POINTER(ctypes.c_uint8)),
    ]


class i2c_rdwr_ioctl_data(ctypes.Structure):
    _fields_ = [
        ("msgs", ctypes.POINTER(i2c_msg)),
        ("nmsgs", ctypes.c_uint32),
    ]


def read_reg(bus, addr, reg, count):
    fd = os.open(f"/dev/i2c-{bus}", os.O_RDWR)
    try:
        wbuf = (ctypes.c_uint8 * 2)(reg >> 8, reg & 0xFF)
        rbuf = (ctypes.c_uint8 * count)()

        msgs = (i2c_msg * 2)()
        msgs[0].addr = addr
        msgs[0].flags = 0
        msgs[0].len = 2
        msgs[0].buf = ctypes.cast(wbuf, ctypes.POINTER(ctypes.c_uint8))

        msgs[1].addr = addr
        msgs[1].flags = I2C_M_RD
        msgs[1].len = count
        msgs[1].buf = ctypes.cast(rbuf, ctypes.POINTER(ctypes.c_uint8))

        data = i2c_rdwr_ioctl_data()
        data.msgs = msgs
        data.nmsgs = 2

        fcntl.ioctl(fd, I2C_RDWR, data)
        return bytes(rbuf)
    finally:
        os.close(fd)


if __name__ == "__main__":
    bus = int(sys.argv[1])
    addr = int(sys.argv[2], 16)
    reg = int(sys.argv[3], 16)
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 2
    val = read_reg(bus, addr, reg, count)
    print(f"reg 0x{reg:04x} = {val.hex()}")
