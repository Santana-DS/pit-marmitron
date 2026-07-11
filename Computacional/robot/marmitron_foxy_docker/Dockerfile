FROM osrf/ros:foxy-desktop-focal

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_PREFIX_PATH=/opt/ros/foxy
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
    ros-foxy-cv-bridge \
    ros-foxy-image-transport \
    ros-foxy-v4l2-camera \
    ros-foxy-usb-cam \
    ros-foxy-rclcpp \
    ros-foxy-sensor-msgs \
    ros-foxy-geometry-msgs \
    ros-foxy-nav-msgs \
    ros-foxy-message-filters \
    ros-foxy-rviz2 \
    ros-foxy-robot-localization \
    ros-foxy-navigation2 \
    ros-foxy-nav2-bringup \
    ros-foxy-libg2o \
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
RUN source /opt/ros/foxy/setup.bash && \
    apt-get update && rosdep update && \
    cd /stella_dependencias && \
    git clone --recursive --depth 1 https://github.com/stella-cv/stella_vslam.git && \
    rosdep install -y -i --from-paths /stella_dependencias/stella_vslam --skip-keys=libg2o && \
    cd /stella_dependencias/stella_vslam && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j$(($(nproc) / 2)) && \
    make install && \
    cd /stella_dependencias && \
    git clone --recursive https://github.com/stella-cv/iridescence_viewer.git && \
    mkdir -p iridescence_viewer/build && cd iridescence_viewer/build && \
    cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    make -j$(($(nproc) / 2)) && \
    make install
    
#Stella-VSLAM-Ros
RUN source /opt/ros/foxy/setup.bash && \
    mkdir -p /marmitron_ws/src && cd /marmitron_ws/src && \
    git clone --recursive -b ros2 --depth 1 https://github.com/stella-cv/stella_vslam_ros.git && \
    
    # 1. Ajustes de Headers
    find stella_vslam_ros/src -type f -exec sed -i 's/<tf2_eigen\/tf2_eigen.hpp>/<tf2_eigen\/tf2_eigen.h>/g' {} + && \
    find stella_vslam_ros/src -type f -exec sed -i 's/<tf2_geometry_msgs\/tf2_geometry_msgs.hpp>/<tf2_geometry_msgs\/tf2_geometry_msgs.h>/g' {} + && \
    
    # 2. Correção de NodePtr para SharedPtr
    find stella_vslam_ros/src -type f -exec sed -i 's/typename message_filters::Subscriber<M>::NodePtr/rclcpp::Node::SharedPtr/g' {} + && \
    
    # 3. Remover funcionalidades problemáticas de QOS que falham no linker do Foxy
    # Remove as chamadas de evento de QOS que causam "undefined reference"
    sed -i '/get_number_of_ready_events/d' stella_vslam_ros/src/stella_vslam_ros.cc || true && \
    
    # 4. Forçar o CMake a linkar corretamente o rclcpp
    sed -i '/target_link_libraries(/a \ \ \ \ rclcpp' stella_vslam_ros/src/CMakeLists.txt && \
    
    # 5. Build
    cd /marmitron_ws && \
    rosdep install -y -i --from-paths /marmitron_ws/src --skip-keys=stella_vslam && \
    colcon build --symlink-install --cmake-args -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=1" && \
    rm -rf /var/lib/apt/lists/*
    
#Resto das instalações no marmitron_ws:
RUN cd /marmitron_ws/src && source /opt/ros/foxy/setup.bash && \
    git clone https://github.com/gabrielconceicao09-coder/serial_comms && \
    cd /marmitron_ws && \
    rosdep install -y -i --from-paths src/serial_comms && \
    colcon build --symlink-install --packages-select serial_comms && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /marmitron_ws

RUN echo "source /opt/ros/foxy/setup.bash" >> ~/.bashrc && \
    echo "source /marmitron_ws/install/setup.bash" >> ~/.bashrc
