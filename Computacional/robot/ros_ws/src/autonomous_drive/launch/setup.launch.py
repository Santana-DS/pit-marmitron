import os
from ament_index_python.packages import get_package_share_directory

from launch_ros.actions import Node
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.actions import IncludeLaunchDescription
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource


def generate_launch_description():
    # Setando parametros e definindo caminhos
    Package_Name_self = "autonomous_drive"
    Package_share_dir = get_package_share_directory(Package_Name_self)
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')

    Launcher_Nav2 = os.path.join(nav2_bringup_dir,  'launch', 'navigation_launch.py')

    param_file_nav2 = os.path.join(Package_share_dir, 'configs', 'nav2_params.yaml')

    param_file_slam = os.path.join(Package_share_dir, 'configs', 'mapper_params_online_async.yaml')

    use_sim_time = LaunchConfiguration('use_sim_time')
    params_ekf_map = [os.path.join(Package_share_dir,"configs","ekf_map.yaml"), {'use_sim_time': use_sim_time}]
    params_ekf_odom = [os.path.join(Package_share_dir,"configs","ekf_odom.yaml"), {'use_sim_time': use_sim_time}]


    # Definindo os executaveis e nós a serem lançados
    
    Nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(Launcher_Nav2),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'params_file': param_file_nav2,
        }.items()
    )

    Slam_Tool_launch = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[param_file_slam,{'use_sim_time': use_sim_time}]
    )

    ekf_node_odom = Node(
        package = 'robot_localization',
        executable = 'ekf_node',
        name = 'ekf_filter_node',
        output = 'screen',
        parameters= params_ekf_odom,
        remappings=[
            ('odometry/filtered', 'odometry/filtered_map')
        ]
    )

    ekf_node_map = Node(
        package = 'robot_localization',
        executable = 'ekf_node',
        name = 'ekf_filter_map_node',
        output = 'screen',
        parameters= params_ekf_map,
         remappings=[
            ('odometry/filtered', 'odometry/global')
        ]
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=['-d', os.path.join(Package_share_dir,'configs', 'config.rviz')],
        parameters=[{'use_sim_time': use_sim_time}]
    )

    navsat_node = Node(
        package='robot_localization',
        executable='navsat_transform_node',
        name='navsat_transform',
        output='screen',
        parameters= params_ekf_map,
        remappings=[
            ('/gps/fix', '/gps/fix'),
            ('/odometry/filtered', '/odometry/filtered_map'),
            ('/odometry/gps', '/odometry/gps')
        ]
    )

    return LaunchDescription([
        DeclareLaunchArgument(name='use_sim_time', default_value='true', description='Use sim time if true'),
        ekf_node_map,
        ekf_node_odom,
        TimerAction(period=6.0, actions=[navsat_node]),
        TimerAction(period=7.0, actions=[Slam_Tool_launch]),
        TimerAction(period=10.0, actions=[Nav2_launch]),   
        rviz_node,
    ])