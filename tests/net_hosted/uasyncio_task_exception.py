import sys
sys.path.append('../../extmod')

try:
    import uasyncio as asyncio
except ImportError:
    try:
        import asyncio
    except ImportError:
        print("SKIP")
        raise SystemExit

async def bar():
    ev = asyncio.Event()
    while True:
        try:
            await e.wait()
        except:
            return

async def foo(n):
    if n == 1:
        raise OSError
    return asyncio.create_task(bar())

async def test():
    async def task(n):
        print("create task server", n)
        srv = await foo(n)
        await srv

        # This should be unreachable.
        print("task finished")

    # Test that cancelling the task will still raise CancelledError.
    for num_sleep in range(2):
        print("sleep", num_sleep)
        t = asyncio.create_task(task(num_sleep))
        for _ in range(num_sleep):
            await asyncio.sleep(0)
        t.cancel()
        try:
            await t
        except asyncio.CancelledError:
            print("CancelledError")

    print("done")


asyncio.run(test())
