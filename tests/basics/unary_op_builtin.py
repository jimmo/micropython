try:
    import uctypes
except:
    print("SKIP")
    raise SystemExit

# check user instances derived from builtin without unary op handling for bool.
class A(uctypes.struct):
    def __init__(self):
        super().__init__(1, 'i')

print(not A())
print(True if A() else False)
