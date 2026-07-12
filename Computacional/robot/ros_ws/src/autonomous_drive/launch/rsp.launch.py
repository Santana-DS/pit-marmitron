import os
from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

from xacro import process_file

# Por existir 2 esqueletos do robo, 1 com os plugin para simulação e 1 para seu esqueleto físico
# é necessário definir tudo em uma função antes que essa seja chamada para o launch
# para que, a file seja carregada antes da execução 
def launch_setup(context, *args, **kwargs):

    #Pegando os parametros de sim time e esqueleto do robo
    use_sim_time = LaunchConfiguration('use_sim_time')
    design_robo = LaunchConfiguration('URDF_file').perform(context)

    pkg_path = get_package_share_directory('autonomous_drive')
    xacro_file = os.path.join(pkg_path, 'description', design_robo)
    robot_description_config = process_file(xacro_file).toxml()

    params = {'robot_description': robot_description_config, 'use_sim_time': use_sim_time}
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[params]
    )


    return [node_robot_state_publisher]


def generate_launch_description():

    return LaunchDescription([
        DeclareLaunchArgument(name='use_sim_time', default_value='true', description='caso não definido usa Sim_Time por default'),
        DeclareLaunchArgument(name='URDF_file', default_value='marmitron_base_sim.urdf.xacro', description='Definir o URDF a ser carregado, _sim possui os modelos simulados dos sensores, sem _sim possui apenas o esqueleto'),
        OpaqueFunction(function=launch_setup),
    ])