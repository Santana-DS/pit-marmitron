# Demonstracao Rapida

## Publicar o APK

Prefira anexar o arquivo em uma **GitHub Release**, nao em um commit:

1. Gere `Computacional/app/mobile/build/app/outputs/flutter-apk/MARMITRON_3000-release.apk`.
2. No GitHub, abra **Releases** > **Draft a new release**.
3. Crie a tag `v1.0.0`, envie o APK como arquivo da release e publique.

O Windows pode mostrar um icone generico para arquivos `.apk`. O icone
MARMITRON 3000 aparece depois da instalacao no Android.

## Preparacao MQTT

Os comandos abaixo precisam de `Computacional/app/edge_daemon/.env` com
`MQTT_HOST`, `MQTT_PORT`, `MQTT_USER`, `MQTT_PASSWORD` e `MQTT_CLIENT_ID`.

Abra um terminal em `Computacional/app`.

## Demonstracao visual segura

Este comando publica telemetria sintetica com pose `x/y/theta`, atualiza a
trilha no mapa do operador e o display ESP32. Ele **nao** publica
`robot/commands/navigate`, nao chama ROS e nao movimenta o robo.

```powershell
python -m edge_daemon.sim_command demo --duration-seconds 45 --destination SIM_DEMO
```

Para acompanhar os topicos MQTT em outro terminal:

```powershell
python -m edge_daemon.sim_command listen --seconds 60
```

No app, apresente o mapa ROS local, o progresso, bateria, velocidade e ETA.
No Wokwi, apresente o painel de navegacao, seguido do estado `CHEGOU`.

## Display no Wokwi

1. Compile `Computacional/robot/MotorMarmita` no PlatformIO.
2. No Wokwi, abra `Computacional/robot/MotorMarmita/diagram.json`.
3. Em `wokwi.toml`, confira se `firmware` e `elf` apontam para o ambiente
   compilado em `.pio/build/<ambiente>/firmware.elf`.
4. Inicie a simulacao e execute o comando `demo` acima.

O `lock_secrets.h` do Wokwi deve conter as credenciais MQTT de demonstracao.
Ele nao deve ser enviado ao GitHub.

## Teste ROS/Nav2 por pose local

Use somente com Gazebo/Nav2 ou com autorizacao da equipe de Computacao. Este
comando publica `robot/commands/navigate`; o edge daemon conectado recebe a
meta `x/y/theta` no frame `map`.

```powershell
python -m edge_daemon.sim_command navigate --order-id SIM-001 --x 1.0 --y 2.0 --theta 0.0 --frame map --waypoint SIM_POINT
```

O esperado e Nav2 aceitar a meta, o daemon publicar `NAVIGATING` e a trilha
aparecer no painel. Para parar o teste:

```powershell
python -m edge_daemon.sim_command estop --reason demonstracao
```

Para rotas GPS ordenadas, consulte `SIMULATED_ROUTE_DEMO.md` e
`ROS2_EDGE_VALIDATION.md`.
