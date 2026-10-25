"""Launch the MAVLink v2 bridge with the vehicle calibration file."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('mecanum_mavlink'), 'config', 'vehicle.yaml'
    )
    return LaunchDescription([
        Node(
            package='mecanum_mavlink',
            executable='bridge',
            name='mecanum_mavlink',
            output='screen',
            parameters=[config],
        ),
    ])
