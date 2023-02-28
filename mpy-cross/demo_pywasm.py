import pywasm
import struct
import sys

def mpycross_error(store, buf, len):
    print("error", len)
    raise ValueError()

def proc_exit(i):
    print("proc_exit")

def fd_close(i):
    return 0

def fd_write(a, b, c, d):
    return 0

def fd_seek(a, b, c, d):
    return 0

runtime = pywasm.load('build/mpy-cross.wasm', {
                    'env': {
                        'mpycross_error': mpycross_error,
                    },
                    'wasi_snapshot_preview1': {
                        'proc_exit': proc_exit,
                        'fd_close': fd_close,
                        'fd_write': fd_write,
                        'fd_seek': fd_seek,
                    },
                })

with open(sys.argv[1], "rb") as f:
    PY = f.read()
    F = sys.argv[1].encode() + b"\x00"

mem = runtime.store.memory_list[0].data


import time
start_time = time.time()

for i in range(10):
    input_name = runtime.exec("demo_malloc", [len(F)])
    mem[input_name:input_name+len(F)] = F

    input_len = len(PY)
    input_data = runtime.exec("demo_malloc", [input_len])
    mem[input_data:input_data+input_len] = PY

    output_len = runtime.exec("demo_malloc", [4])
    result = runtime.exec("demo_malloc", [4])

    output_data = runtime.exec("demo", [input_name, input_data, input_len, output_len, result])
    result, = struct.unpack('<I', mem[result:result+4])
    output_len, = struct.unpack('<I', mem[output_len:output_len+4])
    print(result)
    print(output_len)
    #print(result)
    mpy = mem[output_data:output_data+output_len]
    print(bytes(mpy))

    with open("demo.mpy", "wb") as f:
        f.write(mpy)

print((time.time() - start_time) / 10)







