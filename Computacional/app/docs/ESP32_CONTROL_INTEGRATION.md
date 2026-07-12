# ESP32 Motor Controller + Display Integration

The production firmware to flash on the motor/encoder ESP32 is
`Computacional/robot/MotorMarmita`. It keeps the 50 Hz PID loop on Core 1 and
runs the display, lock actuator, Wi-Fi and MQTT on Core 0.

## Hardware allocation

| Function | GPIO |
| --- | --- |
| Left/right motors | 25, 26, 27, 14 |
| Encoders | 33, 32 |
| Display MOSI/SCLK/CS | 23, 18, 5 |
| Display DC/RST/backlight | 21, 22, 15 |
| Lock actuator control | 4 |

The display uses an ST7789 driver. Do not enable `ILI9341_DRIVER` for the real
board. GPIO 4 must drive the lock through a transistor or MOSFET, with a common
ground and a flyback diode when the lock is inductive. It must remain LOW at
boot.

## Network configuration

Copy `MotorMarmita/include/lock_secrets.h.example` to `lock_secrets.h` and fill
the station Wi-Fi and MQTT credentials. The new file is ignored by git. Without
it, the firmware builds and retains motor control and the local AP, but MQTT
lock commands stay disabled.

The controller uses `WIFI_AP_STA`: the existing `Marmitron` access point and
manual web control remain available while the ESP connects to the broker.

## MQTT contract

The module subscribes to `robot/commands/display_qr` and
`robot/commands/unlock`, validates the shared `order_id`, drives the ST7789 QR
screen, and actuates GPIO 4 for five seconds after a valid unlock command. It
publishes online/offline and periodic heartbeat payloads on
`robot/status/heartbeat`.

No display or MQTT diagnostic text is written to serial. The motor firmware's
921600 baud serial stream remains reserved for encoder/odometry consumers.

## First hardware test

1. Flash with the motors physically lifted or disconnected.
2. Confirm the local AP and manual web controls still respond.
3. Confirm the display reaches its idle screen after MQTT connects.
4. Publish a valid `display_qr` payload, then matching `unlock` payload.
5. Measure GPIO 4 before attaching the physical lock; it should be LOW except
   for the five-second unlock interval.
