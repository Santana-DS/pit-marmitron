# pit-marmitron
Projeto de robô autônomo de delivery de marmitas chamado "Marmitron 3000", para a disciplina Projeto Integrador de Tecnologias (PIT), 2026.1, Universidade de Brasília
[Baixar APK do MARMITRON 3000](https://github.com/Santana-DS/pit-marmitron/releases/latest/download/MARMITRON_3000-release.apk)

## O APP contém:

- Aplicativo Flutter com perfis de cliente, restaurante e operador.
- Painel do operador com telemetria, mapa ROS local, estado de navegacao,
  parada de emergencia e modulo de camera preparado para integracao.
- Gateway Go, PostgreSQL e rotas de entrega ordenadas.
- Edge daemon ROS 2/Nav2 com comandos MQTT, rota GPS e E-stop.
- Firmware ESP32 do display com QR, desbloqueio e status de navegacao.

## Demonstracao

As instrucoes para demonstracao MQTT, Wokwi e ROS/Nav2 estao em:

[Guia de demonstração: APK, MQTT, Wokwi e ROS/Nav2](Computacional/app/docs/DEMONSTRATION_QUICKSTART.md)

