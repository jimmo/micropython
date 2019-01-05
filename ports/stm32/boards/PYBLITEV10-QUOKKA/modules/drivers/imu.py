# https://github.com/micropython-IMU/micropython-mpu9x50/blob/master/imu.py

from utime import sleep_ms
from machine import I2C
from drivers.vector3d import Vector3d


class MPUException(OSError):
    pass


def bytes_toint(msb, lsb):
    if not msb & 0x80:
        return msb << 8 | lsb
    return - (((msb ^ 255) << 8) | (lsb ^ 255) + 1)


class MPU9250(object):
    _I2Cerror = "I2C failure when communicating with IMU"
    _mpu_addr = (104, 105)
    _mag_addr = 12
    _chip_id = 113

    def __init__(self, side_str, device_addr=None, transposition=(0, 1, 2), scaling=(1, 1, 1)):
        self.accel = Vector3d(transposition, scaling, self._accel_callback)
        self.gyro = Vector3d(transposition, scaling, self._gyro_callback)
        self.mag = Vector3d(transposition, scaling, self._mag_callback)
        self.buf1 = bytearray(1)
        self.buf2 = bytearray(2)
        self.buf3 = bytearray(3)
        self.buf6 = bytearray(6)

        sleep_ms(200)
        if isinstance(side_str, str):
            self._mpu_i2c = I2C(side_str)
        elif hasattr(side_str, 'readfrom'):
            self._mpu_i2c = side_str
        else:
            raise ValueError("Invalid I2C instance")

        if device_addr is None:
            devices = set(self._mpu_i2c.scan())
            mpus = devices.intersection(set(self._mpu_addr))
            number_of_mpus = len(mpus)
            if number_of_mpus == 0:
                raise MPUException("No MPU's detected")
            elif number_of_mpus == 1:
                self.mpu_addr = mpus.pop()
            else:
                raise ValueError("Two MPUs detected: must specify a device address")
        else:
            if device_addr not in (0, 1):
                raise ValueError('Device address must be 0 or 1')
            self.mpu_addr = self._mpu_addr[device_addr]

        self.chip_id()

        self.wake()
        self.set_passthrough(True)
        self.set_accel_range(0)
        self.set_gyro_range(0)

        self.set_accel_filter_range(0)
        self.set_gyro_filter_range(0)
        self._mag_stale_count = 0
        self.mag_correction = self._magsetup()
        self._mag_callback()

    def _read(self, buf, memaddr, addr):
        self._mpu_i2c.readfrom_mem_into(addr, memaddr, buf)

    def _write(self, data, memaddr, addr):
        self.buf1[0] = data
        self._mpu_i2c.writeto_mem(addr, memaddr, self.buf1)

    def wake(self):
        try:
            self._write(0x01, 0x6B, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        return 'awake'

    def sleep(self):
        try:
            self._write(0x40, 0x6B, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        return 'asleep'

    def chip_id(self):
        try:
            self._read(self.buf1, 0x75, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        chip_id = int(self.buf1[0])
        if chip_id != self._chip_id:
            raise ValueError('Bad chip ID retrieved: MPU communication failure')
        return chip_id

    def passthrough(self):
        try:
            self._read(self.buf1, 0x37, self.mpu_addr)
            return self.buf1[0] & 0x02 > 0
        except OSError:
            raise MPUException(self._I2Cerror)

    def set_passthrough(self, mode):
        if type(mode) is bool:
            val = 2 if mode else 0
            try:
                self._write(val, 0x37, self.mpu_addr)
                self._write(0x00, 0x6A, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('pass either True or False')

    def sample_rate(self):
        try:
            self._read(self.buf1, 0x19, self.mpu_addr)
            return self.buf1[0]
        except OSError:
            raise MPUException(self._I2Cerror)

    def set_sample_rate(self, rate):
        if rate < 0 or rate > 255:
            raise ValueError("Rate must be in range 0-255")
        try:
            self._write(rate, 0x19, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)

    def filter_range(self):
        try:
            self._read(self.buf1, 0x1A, self.mpu_addr)
            res = self.buf1[0] & 7
        except OSError:
            raise MPUException(self._I2Cerror)
        return res

    def set_filter_range(self, filt):
        if filt in range(7):
            try:
                self._write(filt, 0x1A, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('Filter coefficient must be between 0 and 6')

    def accel_range(self):
        try:
            self._read(self.buf1, 0x1C, self.mpu_addr)
            ari = self.buf1[0]//8
        except OSError:
            raise MPUException(self._I2Cerror)
        return ari

    def set_accel_range(self, accel_range):
        ar_bytes = (0x00, 0x08, 0x10, 0x18)
        if accel_range in range(len(ar_bytes)):
            try:
                self._write(ar_bytes[accel_range], 0x1C, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('accel_range can only be 0, 1, 2 or 3')

    def gyro_range(self):
        try:
            self._read(self.buf1, 0x1B, self.mpu_addr)
            gri = self.buf1[0]//8
        except OSError:
            raise MPUException(self._I2Cerror)
        return gri

    def set_gyro_range(self, gyro_range):
        gr_bytes = (0x00, 0x08, 0x10, 0x18)
        if gyro_range in range(len(gr_bytes)):
            try:
                self._write(gr_bytes[gyro_range], 0x1B, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('gyro_range can only be 0, 1, 2 or 3')

    def _accel_callback(self):
        try:
            self._read(self.buf6, 0x3B, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        self.accel._ivector[0] = bytes_toint(self.buf6[0], self.buf6[1])
        self.accel._ivector[1] = bytes_toint(self.buf6[2], self.buf6[3])
        self.accel._ivector[2] = bytes_toint(self.buf6[4], self.buf6[5])
        scale = (16384, 8192, 4096, 2048)
        self.accel._vector[0] = self.accel._ivector[0]/scale[self.accel_range()]
        self.accel._vector[1] = self.accel._ivector[1]/scale[self.accel_range()]
        self.accel._vector[2] = self.accel._ivector[2]/scale[self.accel_range()]

    def get_accel_irq(self):
        self._read(self.buf6, 0x3B, self.mpu_addr)
        self.accel._ivector[0] = bytes_toint(self.buf6[0], self.buf6[1])
        self.accel._ivector[1] = bytes_toint(self.buf6[2], self.buf6[3])
        self.accel._ivector[2] = bytes_toint(self.buf6[4], self.buf6[5])

    def _gyro_callback(self):
        try:
            self._read(self.buf6, 0x43, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        self.gyro._ivector[0] = bytes_toint(self.buf6[0], self.buf6[1])
        self.gyro._ivector[1] = bytes_toint(self.buf6[2], self.buf6[3])
        self.gyro._ivector[2] = bytes_toint(self.buf6[4], self.buf6[5])
        scale = (131, 65.5, 32.8, 16.4)
        self.gyro._vector[0] = self.gyro._ivector[0]/scale[self.gyro_range()]
        self.gyro._vector[1] = self.gyro._ivector[1]/scale[self.gyro_range()]
        self.gyro._vector[2] = self.gyro._ivector[2]/scale[self.gyro_range()]

    def get_gyro_irq(self):
        self._read(self.buf6, 0x43, self.mpu_addr)
        self.gyro._ivector[0] = bytes_toint(self.buf6[0], self.buf6[1])
        self.gyro._ivector[1] = bytes_toint(self.buf6[2], self.buf6[3])
        self.gyro._ivector[2] = bytes_toint(self.buf6[4], self.buf6[5])

    def sensors(self):
        return self.accel, self.gyro, self.mag

    def temperature(self):
        try:
            self._read(self.buf2, 0x41, self.mpu_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        return bytes_toint(self.buf2[0], self.buf2[1])/333.87 + 21

    def gyro_filter_range(self):
        try:
            self._read(self.buf1, 0x1A, self.mpu_addr)
            res = self.buf1[0] & 7
        except OSError:
            raise MPUException(self._I2Cerror)
        return res

    def set_gyro_filter_range(self, filt):
        if filt in range(8):
            try:
                self._write(filt, 0x1A, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('Filter coefficient must be between 0 and 7')

    def accel_filter_range(self):
        try:
            self._read(self.buf1, 0x1D, self.mpu_addr)
            res = self.buf1[0] & 7
        except OSError:
            raise MPUException(self._I2Cerror)
        return res

    def set_accel_filter_range(self, filt):
        if filt in range(8):
            try:
                self._write(filt, 0x1D, self.mpu_addr)
            except OSError:
                raise MPUException(self._I2Cerror)
        else:
            raise ValueError('Filter coefficient must be between 0 and 7')

    def _magsetup(self):
        try:
            self._write(0x0F, 0x0A, self._mag_addr)
            self._read(self.buf3, 0x10, self._mag_addr)
            self._write(0, 0x0A, self._mag_addr)
            self._write(0x16, 0x0A, self._mag_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        mag_x = (0.5*(self.buf3[0] - 128))/128 + 1
        mag_y = (0.5*(self.buf3[1] - 128))/128 + 1
        mag_z = (0.5*(self.buf3[2] - 128))/128 + 1
        return (mag_x, mag_y, mag_z)

    def _mag_callback(self):
        try:
            self._read(self.buf1, 0x02, self._mag_addr)
            if self.buf1[0] & 1 == 0:
                return self.mag
            self._read(self.buf6, 0x03, self._mag_addr)
            self._read(self.buf1, 0x09, self._mag_addr)
        except OSError:
            raise MPUException(self._I2Cerror)
        if self.buf1[0] & 0x08 > 0:
            self._mag_stale_count += 1
            return
        self.mag._ivector[1] = bytes_toint(self.buf6[1], self.buf6[0])
        self.mag._ivector[0] = bytes_toint(self.buf6[3], self.buf6[2])
        self.mag._ivector[2] = -bytes_toint(self.buf6[5], self.buf6[4])
        scale = 0.15
        self.mag._vector[0] = self.mag._ivector[0]*self.mag_correction[0]*scale
        self.mag._vector[1] = self.mag._ivector[1]*self.mag_correction[1]*scale
        self.mag._vector[2] = self.mag._ivector[2]*self.mag_correction[2]*scale
        self.mag_stale_count = 0

    def mag_stale_count(self):
        return self._mag_stale_count

    def get_mag_irq(self):
        self._read(self.buf1, 0x02, self._mag_addr)
        if self.buf1[0] == 1:
            self._read(self.buf6, 0x03, self._mag_addr)
            self._read(self.buf1, 0x09, self._mag_addr)
            self.mag._ivector[1] = 0
            if self.buf1[0] & 0x08 == 0:
                self.mag._ivector[1] = bytes_toint(self.buf6[1], self.buf6[0])
                self.mag._ivector[0] = bytes_toint(self.buf6[3], self.buf6[2])
                self.mag._ivector[2] = -bytes_toint(self.buf6[5], self.buf6[4])
