FROM osrf/ros:humble-desktop

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_PREFIX_PATH=/opt/ros/humble
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

RUN apt-get update && apt-get install -y \
    git cmake build-essential pkg-config \
    libeigen3-dev libboost-all-dev \
    libglew-dev libgl1-mesa-dev \
    libopencv-dev libglm-dev \
    libglfw3-dev \
    libpng-dev libjpeg-dev \
    libboost-filesystem-dev libboost-program-options-dev \
    python3-colcon-common-extensions \
    python3-dev \
    nano \
    v4l-utils \
    ros-humble-cv-bridge \
    ros-humble-image-transport \
    ros-humble-v4l2-camera \
    ros-humble-usb-cam \
    ros-humble-rclcpp \
    ros-humble-sensor-msgs \
    ros-humble-geometry-msgs \
    ros-humble-nav-msgs \
    ros-humble-message-filters \
    ros-humble-rviz2 \
    ros-humble-robot-localization \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    && rm -rf /var/lib/apt/lists/*

#Iridescence viewer:
RUN mkdir /stella_dependencias && cd /stella_dependencias && \
    git clone https://github.com/koide3/iridescence.git && \
    cd iridescence && \
    git checkout 085322e0c949f75b67d24d361784e85ad7f197ab && \
    git submodule update --init --recursive && \
    mkdir -p build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j$(($(nproc) / 2)) && \
    sudo make install

#Stella-VSLAM 
RUN source /opt/ros/humble/setup.bash && \
    apt-get update && rosdep update && \
    cd /stella_dependencias && \
    git clone --recursive --depth 1 https://github.com/stella-cv/stella_vslam.git && \
    rosdep install -y -i --from-paths /stella_dependencias/stella_vslam && \
    cd /stella_dependencias/stella_vslam && \
    mkdir -p build && cd build && \
    source /opt/ros/humble/setup.bash && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j$(($(nproc) / 2)) && \
    make install && \
    cd /stella_dependencias && \
    git clone --recursive https://github.com/stella-cv/iridescence_viewer.git && \
    mkdir -p iridescence_viewer/build && cd iridescence_viewer/build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j$(($(nproc) / 2)) && \
    make install && \
    mkdir -p /marmitron_ws/src && cd /marmitron_ws/src && \
    git clone --recursive -b ros2 --depth 1 https://github.com/stella-cv/stella_vslam_ros.git && \
    cd /marmitron_ws && \
    rosdep install -y -i --from-paths /marmitron_ws/src --skip-keys=stella_vslam && \
    colcon build --symlink-install && \
    rm -rf /var/lib/apt/lists/*

#Resto das instalações no marmitron_ws:
RUN cd /marmitron_ws/src && source /opt/ros/humble/setup.bash && \
    git clone https://github.com/gabrielconceicao09-coder/serial_comms && \
    cd /marmitron_ws && \
    rosdep install -y -i --from-paths src/serial_comms && \
    colcon build --symlink-install --packages-select serial_comms && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /marmitron_ws

RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc && \
    echo "source /marmitron_ws/install/setup.bash" >> ~/.bashrc