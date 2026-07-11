FROM osrf/ros:foxy-desktop-focal

SHELL ["/bin/bash", "-c"]

ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_PREFIX_PATH=/opt/ros/foxy
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# =========================
# Dependências base ROS + SLAM
# =========================
RUN apt update && apt install -y \
    git cmake build-essential pkg-config \
    libeigen3-dev libboost-all-dev \
    libglew-dev libgl1-mesa-dev \
    libopencv-dev \
    python3-colcon-common-extensions \
    python3-dev \
    ros-foxy-cv-bridge \
    ros-foxy-image-transport \
    ros-foxy-v4l2-camera \
    ros-foxy-usb-cam \
    ros-foxy-rclcpp \
    ros-foxy-sensor-msgs \
    ros-foxy-message-filters \
    ros-foxy-rviz2 \
    ros-foxy-geometry-msgs \
    ros-foxy-robot-localization \
    && rm -rf /var/lib/apt/lists/*

# =========================
# Pangolin (versão estável SEM python bindings)
# =========================
RUN git clone https://github.com/stevenlovegrove/Pangolin.git /opt/Pangolin && \
    cd /opt/Pangolin && \
    git checkout ad8b5f8 && \
    mkdir build && cd build && \
    cmake .. \
      -DBUILD_PANGOLIN_PYTHON=OFF \
      -DBUILD_PYTHON=OFF \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_TOOLS=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_DISABLE_FIND_PACKAGE_pybind11=ON \
      -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    ldconfig
# garante linker enxergar libpangolin
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/pangolin.conf && ldconfig

# =========================
# ORB-SLAM3
# =========================
RUN cd /opt && \
    git clone https://github.com/zang09/ORB-SLAM3-STEREO-FIXED.git ORB_SLAM3 && \
    cd /opt/ORB_SLAM3 && \
    chmod +x build.sh && \
    ./build.sh
    
#Sophus:
RUN apt update && sudo apt install libfmt-dev && \ 
    cd /opt && \
    rm -rf Sophus && \
    git clone https://github.com/strasdat/Sophus.git && \
    cd Sophus && \
    git checkout 1.22.10 && \
    mkdir build && \ 
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc) && \
    sudo make install && \
    ldconfig

RUN mkdir -p /ws/src

#Instala nano e v4l-utils
RUN apt update && apt install nano && apt install v4l-utils && apt install setserial
    
WORKDIR /ws

#Clona wrapper ORB-SLAM3 modificado
RUN cd /ws/src && \
    git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2 orbslam3_ros2 && \
    cd /ws/src/orbslam3_ros2/vocabulary && tar -xvf ORBvoc.txt.tar.gz

#Clona pacote serial_comms
RUN cd /ws/src && \
    git clone https://github.com/gabrielconceicao09-coder/serial_comms.git

#Monta o pacote
RUN source /opt/ros/foxy/setup.bash && \
    colcon build --executor sequential && \
    source install/setup.bash

#Checa se tudo está nos conformes e configura o que for necessário
RUN echo "source /opt/ros/foxy/setup.bash" >> ~/.bashrc && \
    echo "source /ws/install/setup.bash" >> ~/.bashrc && \
    echo 'export LD_LIBRARY_PATH=/opt/ORB_SLAM3/lib:$LD_LIBRARY_PATH' >> ~/.bashrc && \
    setserial /dev/ttyACM0 low_latency     

CMD ["/bin/bash"]

# Para montar o pkg, precisa incluir /Thirdparty/Sophus no CMakeLists.txt se não tiver encontrando sophus.he
#pra rodar o docker (partindo da pasta da dockerfile): docker run -it --rm --net=host --ipc=host --device=/dev/dri --device=/dev/video2 --device=/dev/ttyUSB0 -e DISPLAY=$DISPLAY -e XDG_RUNTIME_DIR=/tmp/runtime-root -v /tmp/.X11-unix:/tmp/.X11-unix -v /tmp/runtime-root:/tmp/runtime-root --mount type=bind,src=.,dst=/mount,ro marmitron_docker

#como eu tava rodando antes: docker run -it --rm --net=host --ipc=host --privileged --device=/dev/video3 --device=/dev/ttyUSB0 -e DISPLAY=$DISPLAY -e XDG_RUNTIME_DIR=/tmp/runtime-root -v /tmp/.X11-unix:/tmp/.X11-unix -v /tmp/runtime-root:/tmp/runtime-root <TAG DO DOCKER>
#Para incluir o pacote de comunicação serial: cd /ws/src && git clone https://github.com/gabrielconceicao09-coder/serial_comms.git
#Pra atualizar o src do orbslam3_ros2 com o repositorio fork: cd /ws/src && rm -r orbslam3_ros2 && git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2.git orbslam3_ros2
#Se reinstalar o wrapper tem que descompacatar o vocabulary: cd /ws/src/orbslam3_ros2/vocabulary && tar -xf ORBvoc.txt.tar.gz
#Precisa rodar (não lembro mt pq nem como consertar kkkkk): export LD_LIBRARY_PATH=/opt/ORB_SLAM3/lib:$LD_LIBRARY_PATH 
 # Pra chamar os nodes do pacote, precisa do endereço do vocab e da configuração (.yaml):
# Pra referência fácil: ros2 run orbslam3 mono-inertial src/orbslam3_ros2/vocabulary/ORBvoc.txt src/orbslam3_ros2/config/monocular-inertial/EuRoC.yaml
#Pra mostrar as imagens rodando: docker ps
#Pra rodar outro terminal na mesma imagem: docker exec -it <container_id> bash

#Se tiver td certo no código fonte: cd /ws/src && git clone https://github.com/gabrielconceicao09-coder/serial_comms.git && cd /ws/src && rm -r orbslam3_ros2 && git clone https://github.com/gabrielconceicao09-coder/ORB_SLAM3_ROS2.git orbslam3_ros2 && cd /ws && colcon build --executor sequential

