# Test basic behaviour of uasyncio.start_server()

try:
    import uasyncio as asyncio
except ImportError:
    try:
        import asyncio
    except ImportError:
        print("SKIP")
        raise SystemExit


async def test():
    # Test creating 2 servers using the same address
    print("create server1")
    server1 = await asyncio.start_server(None, "0.0.0.0", 8000)
    try:
        print("create server2")
        await asyncio.start_server(None, "0.0.0.0", 8000)
    except OSError as er:
        print("OSError")

    # Wait for server to close.
    async with server1:
        print("sleep")
        await asyncio.sleep(0)

    # Test that cancellation works before the server starts if
    # the subsequent code raises.
    print("create server3")
    server3 = await asyncio.start_server(None, "0.0.0.0", 8000)
    try:
        async with server3:
            raise OSError
    except OSError as er:
        print("OSError")

    # Test that closing doesn't raise CancelledError.
    print("create server4")
    server4 = await asyncio.start_server(None, "0.0.0.0", 8000)
    server4.close()
    await server4.wait_closed()
    print("server4 closed")

    async def task(n):
        print("create task server", n)
        srv = await asyncio.start_server(None, "0.0.0.0", 8000 + n)
        await srv.wait_closed()
        # This should be unreachable.
        print("task finished")

    # Test that cancelling the task will still raise CancelledError.
    for num_sleep in range(4):
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
