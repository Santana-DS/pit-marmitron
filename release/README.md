# MARMITRON 3000 v1.0.0

## Artefato Android

O APK de entrega e:

```text
MARMITRON_3000-release.apk
```

Gere-o a partir de `Computacional/app/mobile`:

```powershell
.\tool\build_marmitron_release.ps1
```

O arquivo resultante fica em:

```text
Computacional/app/mobile/build/app/outputs/flutter-apk/MARMITRON_3000-release.apk
```

Anexe esse arquivo a uma **GitHub Release** associada a tag `v1.0.0`.
Nao envie o APK em commits: ele e um binario de distribuicao e aumentaria o
historico do repositorio sem vantagem para desenvolvimento.

## Conteudo da entrega

- Aplicativo Flutter com perfis de cliente, restaurante e operador.
- Painel do operador com telemetria, mapa ROS local, estado de navegacao,
  parada de emergencia e modulo de camera preparado para integracao.
- Gateway Go, PostgreSQL e rotas de entrega ordenadas.
- Edge daemon ROS 2/Nav2 com comandos MQTT, rota GPS e E-stop.
- Firmware ESP32 do display com QR, desbloqueio e status de navegacao.

## Demonstracao

As instrucoes para demonstracao MQTT, Wokwi e ROS/Nav2 estao em:

```text
Computacional/app/docs/DEMONSTRATION_QUICKSTART.md
```

Nao envie `lock_secrets.h`, `.env`, chaves privadas ou senhas para o
repositorio ou para a GitHub Release.
