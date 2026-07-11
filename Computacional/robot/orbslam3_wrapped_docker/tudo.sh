#!/bin/bash
cd /ws/src
#!/bin/bash
FILE1="/ws/src/orbslam3_ros2"
if [ -e "$FILE1" ]
then
	rm -r "$FILE1"
fi
git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2 orbslam3_ros2
FILE2="/ws/src/serial_comms"
if [ -e "$FILE2" ]
then
	rm -r "$FILE2"
fi
git clone https://github.com/gabrielconceicao09-coder/serial_comms.git

cd /ws/src/orbslam3_ros2/vocabulary
tar -xvf ORBvoc.txt.tar.gz
cd /ws
colcon build --executor sequential
source install/setup.bash
