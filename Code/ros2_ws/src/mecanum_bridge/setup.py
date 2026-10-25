from setuptools import setup

setup(
    name='mecanum_bridge', version='0.1.0', packages=['mecanum_bridge'],
    data_files=[('share/ament_index/resource_index/packages', ['resource/mecanum_bridge']),
                ('share/mecanum_bridge', ['package.xml']),
                ('share/mecanum_bridge/config', ['config/vehicle.yaml'])],
    install_requires=['setuptools', 'pyserial'], zip_safe=True,
    maintainer='Mecanum vehicle contributors', maintainer_email='maintainers@example.invalid',
    description='STM32 dual-board serial control and telemetry bridge', license='MIT',
    entry_points={'console_scripts': ['bridge = mecanum_bridge.node:main']},
)
