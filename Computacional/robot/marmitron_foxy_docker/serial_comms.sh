#!/bin/bash
cd /ws/src
FILE="/ws/src/serial_comms"
if [ -e "$FILE" ]
then
	cd "$FILE"
	git pull
else
	git clone https://github.com/gabrielconceicao09-coder/serial_comms.git
fi

cd /ws
colcon build --executor sequential --packages-select serial_comms
source install/setup.bash
