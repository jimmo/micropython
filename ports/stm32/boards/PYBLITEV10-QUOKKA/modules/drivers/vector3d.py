# https://github.com/micropython-IMU/micropython-mpu9x50/blob/master/vector3d.py

from utime import sleep_ms
from math import sqrt, degrees, acos, atan2


def default_wait():
    '''
    delay of 50 ms
    '''
    sleep_ms(50)


class Vector3d(object):
    def __init__(self, transposition, scaling, update_function):
        self._vector = [0, 0, 0]
        self._ivector = [0, 0, 0]
        self.cal = (0, 0, 0)
        self.argcheck(transposition, "Transposition")
        self.argcheck(scaling, "Scaling")
        if set(transposition) != {0, 1, 2}:
            raise ValueError('Transpose indices must be unique and in range 0-2')
        self._scale = scaling
        self._transpose = transposition
        self.update = update_function

    def argcheck(self, arg, name):
        if len(arg) != 3 or not (type(arg) is list or type(arg) is tuple):
            raise ValueError(name + ' must be a 3 element list or tuple')

    def calibrate(self, stopfunc, waitfunc=default_wait):
        self.update()
        maxvec = self._vector[:]
        minvec = self._vector[:]
        while not stopfunc():
            waitfunc()
            self.update()
            maxvec = list(map(max, maxvec, self._vector))
            minvec = list(map(min, minvec, self._vector))
        self.cal = tuple(map(lambda a, b: (a + b)/2, maxvec, minvec))

    def _calvector(self):
        return list(map(lambda val, offset: val - offset, self._vector, self.cal))

    def get_x(self):
        self.update()
        return self._calvector()[self._transpose[0]] * self._scale[0]

    def get_y(self):
        self.update()
        return self._calvector()[self._transpose[1]] * self._scale[1]

    def get_z(self):
        self.update()
        return self._calvector()[self._transpose[2]] * self._scale[2]

    def get_values(self):
        self.update()
        return (self._calvector()[self._transpose[0]] * self._scale[0],
                self._calvector()[self._transpose[1]] * self._scale[1],
                self._calvector()[self._transpose[2]] * self._scale[2])

    def magnitude(self):
        x, y, z = self.get_values()
        return sqrt(x**2 + y**2 + z**2)

    def inclination(self):
        x, y, z = self.get_values()
        return degrees(acos(z / sqrt(x**2 + y**2 + z**2)))

    def elevation(self):
        return 90 - self.inclination()

    def azimuth(self):
        x, y, z = self.get_values()
        return degrees(atan2(y, x))

    def get_ix(self):
        return self._ivector[0]

    def get_iy(self):
        return self._ivector[1]

    def get_iz(self):
        return self._ivector[2]

    def get_ivalues(self):
        return self._ivector

    def transpose(self):
        return tuple(self._transpose)

    def scale(self):
        return tuple(self._scale)
