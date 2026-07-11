"""Launch the Logitech C920 as a ROS 2 camera source.

The launch file is intentionally standalone so the Computacao team can copy it
into their ROS workspace or run it directly with:

    ros2 launch edge_daemon/ros2_camera/launch/c920_camera.launch.py

It assumes v4l2_camera is installed and remaps the raw image topic to the app
contract: /camera/image_raw.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    video_device = LaunchConfiguration("video_device")
    image_width = LaunchConfiguration("image_width")
    image_height = LaunchConfiguration("image_height")
    fps = LaunchConfiguration("fps")
    camera_frame_id = LaunchConfiguration("camera_frame_id")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "video_device",
                default_value="/dev/video0",
                description="Linux V4L2 device path for the C920.",
            ),
            DeclareLaunchArgument(
                "image_width",
                default_value="1280",
                description="Camera image width.",
            ),
            DeclareLaunchArgument(
                "image_height",
                default_value="720",
                description="Camera image height.",
            ),
            DeclareLaunchArgument(
                "fps",
                default_value="30",
                description="Camera frames per second.",
            ),
            DeclareLaunchArgument(
                "camera_frame_id",
                default_value="camera_link",
                description="TF frame id stamped on camera messages.",
            ),
            Node(
                package="v4l2_camera",
                executable="v4l2_camera_node",
                name="c920_camera",
                namespace="camera",
                output="screen",
                parameters=[
                    {
                        "video_device": video_device,
                        "image_size": [image_width, image_height],
                        "time_per_frame": [1, fps],
                        "camera_frame_id": camera_frame_id,
                    }
                ],
                remappings=[
                    ("image_raw", "image_raw"),
                    ("camera_info", "camera_info"),
                ],
            ),
        ]
    )
