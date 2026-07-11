# Pontos de entrega calibrados

Cada ponto de entrega conecta dois sistemas de coordenadas diferentes:

- `latitude` e `longitude`: exibem o ponto no mapa geografico da UnB no app.
- `map_x`, `map_y`, `map_theta` e `map_frame`: sao a pose local enviada pelo gateway ao Nav2 via `robot/commands/navigate`.

Nao envie latitude/longitude diretamente ao Nav2. O frame ROS `map` e local ao mapa SLAM e so pode ser preenchido com valores medidos pela equipe de navegacao.

## Entrega esperada da equipe de navegacao

Para cada local seguro, fornecer uma linha com:

| Campo | Exemplo | Observacao |
| --- | --- | --- |
| `point_key` | `FT_ENTRADA` | Identificador estavel, maiusculo e sem espacos. |
| `label` | `FT - Entrada principal` | Nome exibido ao usuario. |
| `display_address` | `Faculdade de Tecnologia - Entrada` | Texto salvo no pedido. |
| `latitude`, `longitude` | `-15.x`, `-47.x` | Posicao no mapa da UnB. |
| `map_x`, `map_y` | `12.0`, `-3.5` | Metros no frame ROS `map`. |
| `map_theta` | `0.0` | Orientacao final em radianos. |
| `map_frame` | `map` | Frame Nav2/TF validado. |

Os valores entram em `database/seeds/003_delivery_points_template.sql` ou diretamente na tabela `delivery_points`. O gateway aceita o `point_key`, resolve a pose no servidor e publica o comando MQTT; o Flutter nunca calcula nem envia pose ROS.

## Validacao antes da demonstracao

1. Com o mesmo launch ROS usado pelo robo/simulacao, confirmar `tf2_echo map base_link`.
2. Inserir um ponto e chamar `GET /api/delivery-points`; conferir nome e local no app.
3. Criar pedido, selecionar o ponto e conferir o payload MQTT de `robot/commands/navigate`.
4. Confirmar no Nav2 que o objetivo aceito usa o mesmo `map_frame`, `x`, `y` e `theta` medidos.

Enquanto nao houver pontos cadastrados, nenhum destino deve ser despachado por coordenadas placeholder.
