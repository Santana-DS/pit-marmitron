# Mensagens dos Sensores

Mensagens utilizadas por cada sensores, e o que cada uma precisa.

## Header

Antes de detalhar as mensagens de cada sensor, é importante entender o campo `header`, já que esse é um campo obrigatorio para todas.

O `header` contém duas informações essenciais:

- **timestamp**: momento em que o dado foi medido.
- **frame_id**: nome exato de um link usado na árvore TF2 do robô. Esse nome pode ser encontrado na descrição URDF. Em caso de dúvida, consultar um dos Andrés.

---

## Ultrassônicos

**Tipo:** `sensor_msgs/msg/Range`

| Campo | Descrição |
|---|---|
| `header` | Timestamp e frame ID |
| `uint8 radiation_type` | Tipo de radiação. Como usamos ultrassom, o valor é `0` |
| `float32 field_of_view` | Ângulo de abertura do sensor (radianos) |
| `float32 min_range` | Limite mínimo detectado pelo sensor (metros) |
| `float32 max_range` | Limite máximo detectado pelo sensor (metros) |
| `float32 range` | Distância até o objeto (metros). Pode retornar `-inf` ou `+inf` em caso de objetos muito próximos ou ausência de objeto, respectivamente |

---

## IMU

**Tipo:** `sensor_msgs/msg/Imu`

> **Observação 1:** A aceleração e a posição geradas pelo IMU geralmente são propensas a erro, por isso costumam não ser usadas no `robot_localization`. Para simplificar, esses campos podem ser zerados ao publicar a mensagem — mas é necessário avaliar o nosso caso.

> **Observação 2:** Campos que usam outros tipos de mensagem (como `Vector3`) podem ser consultados na documentação oficial. Porém abaixo do primeiro exemplo de cada tipo, inclui um exemplo.

| Campo | Descrição |
|---|---|
| `header` | Timestamp e frame ID |
| `geometry_msgs/Quaternion orientation` | Orientação em quatérnions (`x`, `y`, `z`, `w`). *Verificar se o IMU calcula a orientação; caso não calcule, zerar todos os campos* |
| `float64[9] orientation_covariance` | Matriz 3x3 de covariância da orientação |
| `geometry_msgs/Vector3 angular_velocity` | Velocidade angular nos eixos `x`, `y`, `z` (rad/s) |
| `float64[9] angular_velocity_covariance` | Matriz 3x3 de covariância da velocidade angular |
| `geometry_msgs/Vector3 linear_acceleration` | Aceleração linear nos eixos `x`, `y`, `z` |
| `float64[9] linear_acceleration_covariance` | Matriz 3x3 de covariância da aceleração linear |

**Estrutura de `Vector3`**
|float64 x|
|float64 y|
|float64 z|
---

## GPS

**Tipo:** `sensor_msgs/msg/NavSatFix`

| Campo | Descrição |
|---|---|
| `header` | Timestamp e frame ID |
| `sensor_msgs/NavSatStatus status` | Ver detalhes abaixo |
| `float64 latitude` | Latitude em graus (negativo = sul) |
| `float64 longitude` | Longitude em graus (negativo = oeste) |
| `float64 altitude` | Altitude em metros |
| `float64[9] position_covariance` | Matriz 3x3 de covariância da posição (pode ser necessário definir manualmente) |

**Estrutura de `NavSatStatus`:**

| Campo | Descrição |
|---|---|
| `status.status` | Fornecido pelo próprio GPS |
| `status.service` | Aparentemente precisa ser definido por nós |

> O nó `navsat` converte essa mensagem para o formato utilizado pelo `robot_localization`.

---

## Rodas

**Tipo:** `geometry_msgs/msg/TwistWithCovarianceStamped`

| Campo | Descrição |
|---|---|
| `header` | Timestamp e frame ID |
| `twist` | Do tipo `TwistWithCovariance`, que contém uma mensagem `geometry_msgs/msg/Twist` |

> O `twist` usa a velocidade linear e angular de ambas as rodas juntas.