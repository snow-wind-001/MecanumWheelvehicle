"""Own Pi GPIO17 READY through the Linux GPIO character device.

The firmware treats this line as a hardware interlock: the Pi may only arm or
motion-authorise the vehicle while READY is asserted, and a low READY is also
how a stopping host announces itself. GPIO22 HALTED belongs exclusively to the
kernel gpio-poweroff driver and is never touched here.

Example: line = ReadyLine('/dev/gpiochip0'); line.set(False); line.close()
"""
import importlib


class ReadyLine:
    def __init__(self, chip_path, offset=17):
        if offset != 17:
            raise ValueError('This board assigns READY to BCM GPIO17 only')
        self.request = self.chip = self.line = None
        self.offset = offset
        self.active = False
        gpiod = importlib.import_module('gpiod')
        if hasattr(gpiod, 'request_lines'):
            self.values = gpiod.line.Value
            self.request = gpiod.request_lines(
                chip_path, consumer='mecanum-pi-ready',
                config={offset: gpiod.LineSettings(direction=gpiod.line.Direction.OUTPUT,
                                                  output_value=self.values.INACTIVE)})
        else:
            self.chip = gpiod.Chip(chip_path)
            try:
                self.line = self.chip.get_line(offset)
                self.line.request(consumer='mecanum-pi-ready', type=gpiod.LINE_REQ_DIR_OUT, default_vals=[0])
            except Exception:
                self.chip.close()
                self.chip = self.line = None
                raise

    def set(self, active):
        active = bool(active)
        if self.request is None and self.line is None:
            raise RuntimeError('READY line is closed')
        if active == self.active:
            return
        if self.request is not None:
            self.request.set_value(self.offset, self.values.ACTIVE if active else self.values.INACTIVE)
        else:
            self.line.set_value(int(active))
        self.active = active

    def close(self):
        try:
            if self.request is not None or self.line is not None:
                self.set(False)
        finally:
            if self.request is not None:
                self.request.release()
                self.request = None
            if self.line is not None:
                self.line.release()
                self.line = None
            if self.chip is not None:
                self.chip.close()
                self.chip = None
