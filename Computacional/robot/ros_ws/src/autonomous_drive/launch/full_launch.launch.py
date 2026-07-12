import os
from ament_index_python.packages import get_package_share_directory

from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import TimerAction
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

# Esse launch inicia o gazebo junto da bridge, para assim se visualizar a simulação

def generate_launch_description():

    package_name= 'autonomous_drive'
    package_share = get_package_share_directory(package_name)

    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(package_share, 'launch', 'rsp.launch.py')),
        launch_arguments={'use_sim_time': 'false', 'URDF_file': 'marmitron_base.urdf.xacro'}.items()
    )

    setup = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(package_share, 'launch','setup.launch.py')),
        launch_arguments={'use_sim_time':'False'}.items()
    )

    calc_vel_node = Node(
        package=package_name,
        executable='vel_rodas',
        output='screen',
        name='Vel_rodas',
    )

    Mux = Node(
        package=package_name,
        executable='mux',
        output='screen',
        name='mux',
    )

    return LaunchDescription([
        rsp,
        calc_vel_node,
        Mux,
        TimerAction(period=3.0, actions=[setup]),
    ])