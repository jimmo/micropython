import rp2, machine, time

rp2.bt_init()

u = machine.UART(0, baudrate=115200)

print("running")

while True:
    msg = b""
    if u.any():
        # print(u.read(1))
        # print("s")
        while u.any():
            # print("r")
            msg += u.read(1)
            if not u.any():
                time.sleep_us(50)
        # print("rx:", msg.hex())
        rp2.bt_write(b"\x00\x00\x00" + msg)
    if w := rp2.bt_read():
        u.write(w[3:])
