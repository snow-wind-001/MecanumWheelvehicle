#!/usr/bin/env python3
"""Reject inconsistent generated STM32F429 PLL clocks before compiling.

Example: python3 Code/tools/check_clock_config.py
"""
import re
from pathlib import Path

root=Path(__file__).resolve().parents[1]/'MecanumB_Control'
source=(root/'Src/main.c').read_text()
conf=(root/'Inc/stm32f4xx_hal_conf.h').read_text()
ioc=dict(line.split('=',1) for line in (root/'MecanumB_Control.ioc').read_text().splitlines() if '=' in line)
osc=re.search(r'PLLSource = RCC_PLLSOURCE_(HSE|HSI);',source).group(1)
freq=int(re.search(r'#define\s+'+osc+r'_VALUE\s+\(?\s*(\d+)',conf).group(1))
m=int(re.search(r'PLL\.PLLM = (\d+);',source).group(1))
n=int(re.search(r'PLL\.PLLN = (\d+);',source).group(1))
p=int(re.search(r'PLL\.PLLP = RCC_PLLP_DIV(\d+);',source).group(1))
vco_in=freq/m;vco_out=vco_in*n;sysclk=vco_out/p
assert 1e6<=vco_in<=2e6, f'PLL input out of range: {vco_in}'
assert 100e6<=vco_out<=432e6, f'PLL VCO out of range: {vco_out}'
assert sysclk==180e6, f'Expected 180 MHz, got {sysclk}'
assert ioc['RCC.PLLSourceVirtual']==f'RCC_PLLSOURCE_{osc}'
assert int(ioc['RCC.SYSCLKFreq_VALUE'])==sysclk
assert 'HAL_PWREx_EnableOverDrive()' in source and 'FLASH_LATENCY_5' in source
assert 'RCC_HCLK_DIV4' in source and 'RCC_HCLK_DIV2' in source
print(f'PASS: {osc}={freq}, PLL={m}/{n}/{p}, SYSCLK={sysclk:.0f}, APB1=45 MHz, APB2=90 MHz')
