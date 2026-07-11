#!/bin/bash
cd /ws/src
#!/bin/bash
FILE1="/ws/src/orbslam3_ros2"
if [ -e "$FILE1" ]
then
	cd "$FILE1"
	git pull
else 
	git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2 orbslam3_ros2
fi
FILE2="/ws/src/serial_comms"
if [ -e "$FILE2" ]
then
	cd "$FILE2"
	git pull
else
	git clone https://github.com/gabrielconceicao09-coder/serial_comms.git
fi
cd /ws/src/orbslam3_ros2/vocabulary
tar -xvf ORBvoc.txt.tar.gz
cd /ws
colcon build --executor sequential --cmake-args -DCMAKE_BUILD_TYPE=Debug
source install/setup.bash
