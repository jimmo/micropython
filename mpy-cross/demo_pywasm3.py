import wasm3
import struct
import sys

WASM = open("build/mpy-cross.wasm", "rb").read()

with open(sys.argv[1], "rb") as f:
    PY = f.read()
    F = sys.argv[1].encode() + b"\x00"

env = wasm3.Environment()
rt  = env.new_runtime(204800)
mod = env.parse_module(WASM)
rt.load(mod)
mem = rt.get_memory(0)
wasm_demo_malloc = rt.find_function("demo_malloc")
wasm_demo = rt.find_function("demo")


def mpycross_error(buf, len):
    print(bytes(mem[buf:buf+len]).decode())
    raise ValueError

mod.link_function("env", "mpycross_error", "v(ii)", mpycross_error)

import time
start_time = time.time()

for i in range(80):
    input_name = wasm_demo_malloc(len(F))
    mem[input_name:input_name+len(F)] = F

    input_len = len(PY)
    input_data = wasm_demo_malloc(input_len)
    mem[input_data:input_data+input_len] = PY

    output_len = wasm_demo_malloc(4)
    result = wasm_demo_malloc(4)

    output_data = wasm_demo(input_name, input_data, input_len, output_len, result)
    result, = struct.unpack('<I', mem[result:result+4])
    output_len, = struct.unpack('<I', mem[output_len:output_len+4])
    print(result)
    print(output_len)
    #print(result)
    mpy = mem[output_data:output_data+output_len]
    print(bytes(mpy))

    with open("demo.mpy", "wb") as f:
        f.write(mpy)

print((time.time() - start_time))
