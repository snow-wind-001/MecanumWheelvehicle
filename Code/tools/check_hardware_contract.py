#!/usr/bin/env python3
"""Compare the archived PCB2 MCU nets with firmware pin definitions.

Example: python3 Code/tools/check_hardware_contract.py
"""
import csv
import hashlib
import json
from pathlib import Path
import re
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ARCHIVE = ROOT / 'PCB/backups/PCB3_1_routing_complete.epro2'
BOARD = '9fb4ea4c3f4a2c99'
MCU = '49a5ef5495958206'
ALIASES = {'ESP32_TX': 'ESP_MCU_TX', 'ESP32_RX': 'ESP_MCU_RX'}
# These optional functions have no MCU copper net in the archived PCB2.
UNWIRED = {'LD06_TX', 'POSE_TX', 'POSE_RX', 'LOG_RX', 'LOG_TX'}


def main():
    nets = {}
    active = False
    board_uuid = None
    board_nets = {}
    refs = {}
    with zipfile.ZipFile(ARCHIVE) as archive:
        source = next(n for n in archive.namelist() if n.endswith('.epru'))
        for line in archive.read(source).decode().splitlines():
            try:
                header, body = line.split('||', 1)
                header = json.loads(header)
                body = json.JSONDecoder().raw_decode(body.lstrip())[0]
            except (ValueError, json.JSONDecodeError):
                continue
            if header.get('type') == 'DOCHEAD':
                board_uuid = body.get('uuid')
                active = board_uuid in (BOARD, 'd5bab27b5a49d1b7')
            if active and header.get('type') == 'PAD_NET':
                key = json.loads(header['id'])
                board_nets[(board_uuid, key[1], str(key[2]))] = body['padNet']
                if board_uuid == BOARD:
                    nets[(key[1], str(key[2]))] = body['padNet']
            if active and header.get('type') == 'ATTR' and body.get('key') == 'Designator':
                refs[(board_uuid, body['value'])] = body['parentId']
    if len([k for k in nets if k[0] == MCU]) != 144:
        raise RuntimeError('Archived MCU must contain all 144 pin records')
    header = (ROOT / 'Code/MecanumB_Control/Inc/main.h').read_text()
    pins = dict(re.findall(r'#define (\w+)_Pin GPIO_PIN_(\d+)', header))
    ports = dict(re.findall(r'#define (\w+)_GPIO_Port GPIO([A-K])', header))
    rows = []
    with (ROOT / 'Code/cubemx/B板引脚分配_v1.csv').open(encoding='utf-8-sig') as handle:
        for row in csv.DictReader(handle):
            role = row['网络名']
            actual = nets.get((MCU, row['LQFP144物理脚']), '')
            expected = ALIASES.get(role, role)
            status = 'optional_unwired' if role in UNWIRED and not actual else ('pass' if actual == expected else 'FAIL')
            if role in pins and role in ports:
                if row['MCU引脚'] != 'P' + ports[role] + pins[role]:
                    status = 'FAIL'
            rows.append({'role': role, 'port': row['MCU引脚'], 'package_pin': row['LQFP144物理脚'],
                         'archived_net': actual, 'status': status})
    reset_ok = nets.get(('7478834ae81dfa86', '55')) == 'NRST_B' and nets.get((MCU, '25')) == 'NRST_B'
    harness_errors = []
    manifest = json.loads((ROOT / 'PCB/design/AB_INTERCONNECT_v1.json').read_text())
    for pin, channel in manifest['channels'].items():
        for uuid, ref, side in [('d5bab27b5a49d1b7', 'J8', 'a'), (BOARD, 'J15', 'b')]:
            actual = board_nets.get((uuid, refs[(uuid, ref)], str(pin)), '')
            if actual != (channel[f'{side}_net'] or ''):
                harness_errors.append(f'{ref}.{pin}: {actual} != {channel[f"{side}_net"]}')
    power = {'1': '+5V_PI', '2': '+5V_PI', '3': 'GND', '4': 'GND',
             '5': '+5V_AUX', '6': 'GND', '7': '3V3_CTRL', '8': 'GND'}
    for uuid, ref in [('d5bab27b5a49d1b7', 'J9'), (BOARD, 'J16')]:
        for pin, expected in power.items():
            actual = board_nets.get((uuid, refs[(uuid, ref)], pin), '')
            if actual != expected:
                harness_errors.append(f'{ref}.{pin}: {actual} != {expected}')
    result = {'archive': str(ARCHIVE.relative_to(ROOT)), 'sha256': hashlib.sha256(ARCHIVE.read_bytes()).hexdigest(),
              'pcb_uuid': BOARD, 'mcu': 'U10 STM32F429ZGT6', 'rows': rows,
              'watchdog_connector_55_to_nrst_25': reset_ok,
              'harness': {'signal_positions': 60, 'power_positions': 8, 'errors': harness_errors},
              'failures': sum(row['status'] == 'FAIL' for row in rows) + (not reset_ok) + len(harness_errors),
              'scope': 'Archived PCB2 nets, pin baseline, and GPIO macros; not live EDA or measured wiring.'}
    output = ROOT / 'Code/reports/hardware_contract_v1.json'
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({k: v for k, v in result.items() if k != 'rows'}, ensure_ascii=False, indent=2))
    print('Required matching pins:', sum(r['status'] == 'pass' for r in rows), '; optional unwired:', len(UNWIRED))
    return bool(result['failures'])


if __name__ == '__main__':
    raise SystemExit(main())
