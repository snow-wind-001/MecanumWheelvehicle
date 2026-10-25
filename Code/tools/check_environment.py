#!/usr/bin/env python3
"""Check local development prerequisites without moving or powering hardware.

Example: python3 Code/tools/check_environment.py --build
"""
import argparse
import json
import shutil
import socket
import subprocess
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='store_true')
    args = parser.parse_args()
    (ROOT/'Code/reports').mkdir(parents=True, exist_ok=True)
    checks = {}
    for name in ('arm-none-eabi-gcc', 'arm-none-eabi-objcopy', 'arm-none-eabi-size', 'make', 'cmake', 'colcon', 'ros2', 'openocd', 'STM32_Programmer_CLI'):
        checks[name] = shutil.which(name)
    blender = Path.home() / 'software/blender-5.2.0-linux-x64/blender'
    checks['blender'] = shutil.which('blender') or (str(blender) if blender.is_file() else None)
    for name, port in [('blender_socket',9876), ('eda_socket',49620)]:
        try:
            with socket.create_connection(('127.0.0.1',port),timeout=1):checks[name]=True
        except OSError:checks[name]=False
    try:
        with urllib.request.build_opener(urllib.request.ProxyHandler({})).open('http://127.0.0.1:49620/health',timeout=2) as response:
            checks['eda_health']=json.load(response)
    except OSError as exc:checks['eda_health']={'error':str(exc)}
    health=checks['eda_health']
    checks['eda_connected']=bool(health.get('service')=='easyeda-bridge' and health.get('status')=='ok' and health.get('edaConnected'))
    checks['freertos_kernel']=(ROOT/'Code/MecanumB_Control/Middlewares/Third_Party/FreeRTOS/Source/tasks.c').exists()
    checks['serial_devices']=[str(p) for pattern in ('ttyACM*','ttyUSB*') for p in Path('/dev').glob(pattern)]
    ros_python=Path.home()/'.venvs/mecanum-ros2/bin/python'
    check=subprocess.run([str(ros_python),'-c','import rclpy,serial; from geometry_msgs.msg import Twist; print("OK")'],text=True,capture_output=True) if ros_python.exists() else None
    checks['ros_python_imports']=bool(check and check.returncode==0)
    checks['ros_python']=str(ros_python)
    if args.build:
        result=subprocess.run(['make','-C',str(ROOT/'Code/MecanumB_Control'),'-j4'],capture_output=True,text=True)
        (ROOT/'Code/reports/firmware_build.log').write_text(result.stdout+result.stderr)
        checks['firmware_build_exit']=result.returncode
    report={'utc':datetime.now(timezone.utc).isoformat(),'checks':checks,
            'limitations':['TCP health is not a substitute for the recorded Blender MCP execute smoke test.',
                           'Compile success does not establish peripheral timing, sensor correctness or motor safety.',
                           'No hardware is flashed or commanded by this script.']}
    path=ROOT/'Code/reports/environment_v1.json';path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))
    required=('arm-none-eabi-gcc','arm-none-eabi-objcopy','arm-none-eabi-size','make','freertos_kernel','ros_python_imports','blender','blender_socket','eda_connected')
    return 0 if all(checks[k] for k in required) and checks.get('firmware_build_exit',0)==0 else 1


if __name__=='__main__':raise SystemExit(main())
