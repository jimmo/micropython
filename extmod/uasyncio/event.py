# MicroPython uasyncio module
# MIT license; Copyright (c) 2019-2020 Damien P. George

from . import core

import io

# Event class for primitive events that can be waited on, set, and cleared
class Event:
    def __init__(self):
        self.state = False  # False=unset; True=set
        self.waiting = core.TaskQueue()  # Queue of Tasks waiting on completion of this event

    def is_set(self):
        return self.state

    def set(self):
        # Event becomes set, schedule any tasks waiting on it
        while self.waiting.peek():
            core._task_queue.push_head(self.waiting.pop_head())
        self.state = True

    def clear(self):
        self.state = False

    async def wait(self):
        if not self.state:
            # Event not set, put the calling task on the event's waiting queue
            self.waiting.push_head(core.cur_task)
            # Set calling task's data to the event's queue so it can be removed if needed
            core.cur_task.data = self.waiting
            yield
        return True


_MP_STREAM_POLL = const(3)

# Event class that can be used from IRQs.
class PollingEvent(io.IOBase):
    def __init__(self):
        self._flag = 0

    def ioctl(self, req, flags):
        if req == _MP_STREAM_POLL:
            return self._flag * flags
        return None

    def set(self):
        self._flag = 1

    async def wait(self):
        yield core._io_queue.queue_read(self)
        self._flag = 0
