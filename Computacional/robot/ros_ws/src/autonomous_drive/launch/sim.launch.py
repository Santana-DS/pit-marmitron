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
    pkg_gazebo = get_package_share_directory('gazebo_ros')
    world_file = os.path.join(get_package_share_directory(package_name), "worlds","world.sdf")

    rsp = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(package_share, 'launch', 'rsp.launch.py')),
        launch_arguments={'use_sim_time': 'true', 'URDF_file': 'marmitron_base_sim.urdf.xacro'}.items()
    )

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_gazebo, 'launch', 'gazebo.launch.py')),
        launch_arguments={'world': world_file}.items()
    )

    spawn_entity = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=[
            '-topic', 'robot_description',
            '-entity', 'marmitron',
            '-z', '0.05'
            ],
        output='screen'
    )

    setup = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(package_share, 'launch','setup.launch.py')),
        launch_arguments={'use_sim_time':'True'}.items()
    )

    calc_vel_node = Node(
        package=package_name,
        executable='vel_rodas',
        output='screen',
        name='Vel_rodas',
    )

    gps_translation = Node(
        package='autonomous_drive',
        executable='pos_gps',
        parameters=[{
            'datum_lat': -15.0,  # Your robot's origin latitude
            'datum_lon': -47.0,  # Your robot's origin longitude
            'update_rate': 5.0
        }]
    )

    Mux = Node(
        package=package_name,
        executable='mux',
        output='screen',
        name='mux',
    )

    
    gps = Node(
        package='autonomous_drive',
        executable='ground_truth_publisher',
        output='screen',
        name='gps_test',
        parameters=[{
            'link_name': 'base_link',
            'gaussian_noise': 0.5,
            'update_rate': 5.0
            }]
    )

    return LaunchDescription([
        rsp,
        calc_vel_node,
        gps,
        gps_translation,
        Mux,
        TimerAction(period=1.0, actions=[gazebo]),
        TimerAction(period=2.0, actions=[spawn_entity]),
        TimerAction(period=8.0, actions=[setup]),
    ])