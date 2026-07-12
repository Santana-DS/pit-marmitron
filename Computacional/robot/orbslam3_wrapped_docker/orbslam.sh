#!/bin/bash
cd /ws/src
#!/bin/bash
FILE="/ws/src/orbslam3_ros2"
if [ -e "$FILE" ]
then
	cd "$FILE"
	git pull
else
	git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2 orbslam3_ros2	
fi
cd /ws/src/orbslam3_ros2/vocabulary
tar -xvf ORBvoc.txt.tar.gz
cd /ws
colcon build --executor sequential --packages-select orbslam3
source install/setup.bash
