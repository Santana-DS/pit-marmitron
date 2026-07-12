# Auditoria de navegacao e hardware

## Material localizado

- O checkout do worktree contem o Docker ORB-SLAM3, mas o workspace de simulacao atualizado esta no repositorio principal em `Computacional/robot/ros_ws/src/autonomous_drive` e ainda precisa ser sincronizado para esta branch.
- `autonomous_drive/launch/setup.launch.py` inicia Nav2, SLAM Toolbox, EKFs e `robot_localization/navsat_transform_node`; portanto a action `navigate_to_pose` e a conversao GPS sao parte da simulacao atual.
- `autonomous_drive/launch/sim.launch.py` publica `/gps/fix` com o node `pos_gps`; `nav2_params.yaml` usa `map` como frame global e `base_link` como frame do robo.
- `robot/Marmitron` le GPS NEO-6M, IMU e sonar e imprime dados no serial. Ainda falta confirmar/publicar a ponte serial real que converte esse GPS em `sensor_msgs/NavSatFix` no hardware.

## Consequencia para route_id

Podemos implementar no `edge_daemon` o orquestrador de rota (validacao de `route_id`, sequenciamento, cancelamento, estados MQTT e E-stop) contra a simulacao Nav2 atual. A passagem para hardware continua dependente da ponte serial que publica GPS/IMU reais e da validacao de precisao no campus.

## Acesso necessario

Para concluir o executor, sincronize `Computacional/robot/ros_ws/src/autonomous_drive` do repositorio principal para esta branch. O material no Desktop contem launch, `package.xml`, topicos ativos, arvore TF e a configuracao Nav2 necessarios para a validacao.

## ESP32 de display/trava

O firmware em `app/hardware/esp32-lock` requer display SPI ST7789V, backlight e GPIO de atuador, alem de MQTT. O firmware de sensores ja usa GPIOs 23/18/19/5 para SPI da IMU, 16/17 para GPS, 32/33 para sonar e pinos de encoder. Ele nao deve receber esse firmware sem redesenho de pinos.

Se houver somente duas ESPs, a candidata menos inadequada e a de controle, mas somente apos auditoria do firmware e pinout fisico reais. Um atuador e um display nao devem comprometer a alimentacao ou o loop de controle dos motores. Uma terceira ESP dedicada continua sendo a opcao segura.
