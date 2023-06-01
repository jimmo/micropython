# tests int.bit_length

try:
    int.bit_length
except AttributeError:
    print('SKIP')
    raise SystemExit

for n in (0, 37, 1024, 1<<29, 1<<30, 1<<31, 1<<32, 1<<61, 1<<62, 1<<63, 1<<64):
    for j in (-1921, -302, -3, -2, -1, 0, 1, 2, 3, 1821):
        for s in (-1, 1):
            x = (s * (n+j))
            print(x, x.bit_length())

print((2048).bit_length())
print((-2048).bit_length())

print(int.bit_length(5))
