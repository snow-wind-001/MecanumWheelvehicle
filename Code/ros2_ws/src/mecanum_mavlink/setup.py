from glob import glob
from setuptools import setup

package_name = 'mecanum_mavlink'

setup(
    name=package_name, version='0.1.0', packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/config', glob('config/*.yaml')),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
    ],
    install_requires=['setuptools', 'pyserial', 'pymavlink'], zip_safe=True,
    maintainer='Mecanum vehicle contributors', maintainer_email='maintainers@example.invalid',
    description='MAVLink v2 control and telemetry bridge for the Mecanum STM32 rover',
    license='MIT',
    entry_points={'console_scripts': ['bridge = mecanum_mavlink.bridge:main']},
)
