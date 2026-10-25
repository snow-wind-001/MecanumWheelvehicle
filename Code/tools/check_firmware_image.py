#!/usr/bin/env python3
"""Check the linked SRAM erase function and reserved parameter sectors.

Example: python3 Code/tools/check_firmware_image.py
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'Code/MecanumB_Control/build'


def main():
    elf = BUILD / 'MecanumB_Control.elf'
    binary = BUILD / 'MecanumB_Control.bin'
    symbols = subprocess.check_output(['arm-none-eabi-nm', '-S', str(elf)], text=True)
    symbol = re.search(r'^([0-9a-f]+)\s+([0-9a-f]+)\s+\w\s+flash_erase_ram$', symbols, re.M)
    if not symbol:
        raise RuntimeError('Missing flash_erase_ram symbol')
    address, length = (int(value, 16) for value in symbol.groups())
    disassembly = subprocess.check_output(['arm-none-eabi-objdump', '-d', str(elf),
                                            '--disassemble=flash_erase_ram'], text=True)
    calls = re.findall(r'\sblx?\s+([^\n]+)', disassembly)
    errors = []
    if not (0x20000000 <= address < address + length <= 0x20030000):
        errors.append('Erase function is not entirely in SRAM')
    if calls:
        errors.append('Erase polling function contains calls: ' + ', '.join(calls))
    image = binary.read_bytes()
    if len(image) > 768 * 1024:
        errors.append('Firmware image overlaps reserved parameter sectors')
    result = {'elf': str(elf.relative_to(ROOT)), 'binary_bytes': len(image),
              'binary_sha256': hashlib.sha256(image).hexdigest(),
              'ram_erase_address': hex(address), 'ram_erase_bytes': length,
              'ram_erase_call_instructions': calls, 'errors': errors,
              'scope': 'Linked-image static checks; no Flash, watchdog or power-loss hardware experiment.'}
    (ROOT / 'Code/reports/firmware_image_contract_v1.json').write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
