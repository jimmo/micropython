import sys, struct
from wasmer import engine, wat2wasm, Store, Module, Instance, Function, ImportObject, Type
#from wasmer_compiler_singlepass import Compiler
from wasmer_compiler_cranelift import Compiler

wasm_bytes = open("build/mpy-cross.wasm", "rb").read()


def mpycross_error_impl(buf: int, l: int) -> None:
    print("error", l, bytes(mem[buf:buf+l]).decode())
    raise ValueError

def proc_exit_impl(i: int) -> None:
    print("proc_exit")

def fd_close_impl(i: int) -> int:
    return 0

def fd_write_impl(a: int, b: int, c: int, d: int) -> int:
    return 0

def fd_seek_impl(a: int, b: 'i64', c: int, d: int) -> int:
    return 0

engine = engine.Universal(Compiler)
store = Store(engine)

mpycross_error = Function(store, mpycross_error_impl)
proc_exit = Function(store, proc_exit_impl)
fd_close = Function(store, fd_close_impl)
fd_write = Function(store, fd_write_impl)
fd_seek = Function(store, fd_seek_impl)
import_object = ImportObject()
import_object.register(
    "env",
    {
        "mpycross_error": mpycross_error,
    }
)
import_object.register(
    "wasi_snapshot_preview1",
    {
        "proc_exit": proc_exit,
        "fd_close": fd_close,
        "fd_write": fd_write,
        "fd_seek": fd_seek,
    }
)

module = Module(store, wasm_bytes)
instance = Instance(module, import_object)
wasm_demo = instance.exports.demo
wasm_malloc = instance.exports.demo_malloc
wasm_free = instance.exports.demo_free

with open(sys.argv[1], "rb") as f:
    PY = f.read()
    F = sys.argv[1].encode() + b"\x00"

#print(instance.exports.guest_memory)
mem = instance.exports.memory.uint8_view(0)

import time
start_time = time.time()

for i in range(2000):
    input_name = wasm_malloc(len(F))
    mem[input_name:input_name+len(F)] = F

    input_len = len(PY)
    input_data = wasm_malloc(input_len)
    mem[input_data:input_data+input_len] = PY

    output_len = wasm_malloc(4)
    result = wasm_malloc(4)

    output_data = wasm_demo(input_name, input_data, input_len, output_len, result)
    result, = struct.unpack('<I', bytes(mem[result:result+4]))
    output_len, = struct.unpack('<I', bytes(mem[output_len:output_len+4]))
    print(result)
    print(output_len)
    #print(result)
    mpy = mem[output_data:output_data+output_len]
    print(bytes(mpy))

    with open("demo.mpy", "wb") as f:
        f.write(bytes(mpy))

    wasm_free(input_name)
    wasm_free(input_data)
    wasm_free(output_len)
    wasm_free(result)


print((time.time() - start_time))







