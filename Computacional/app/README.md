# 🤖 UnBot Delivery: Sistema de Logística Autônoma UnB

<img src="./docs/banner.png" width="50%">

O **UnBot Delivery** é uma solução avançada de logística *last-mile* autônoma desenvolvida para o campus da Universidade de Brasília (UnB).

Este repositório (Monorepo) contém a infraestrutura de software ponta-a-ponta: o aplicativo de pedidos (Flutter), o cérebro transacional e roteamento (Gateway Go), a comunicação via nuvem (MQTT) e o firmware de segurança física da tranca (ESP32 em C++).

---

## 📚 Documentação Arquitetural (Memória do Projeto)

Para manter este repositório escalável e facilitar a integração de novos membros (e IAs), toda a complexidade técnica foi isolada na pasta `docs/`. **Leitura obrigatória antes de codar:**

- 🗺️ **[ARCHITECTURE.md](./docs/ARCHITECTURE.md):** Visão geral do sistema, topologia de rede, responsabilidades dos nós e diagrama estrutural.
- 📜 **[PROTOCOL.md](./docs/PROTOCOL.md):** Contratos de API REST, payloads JSON e mapeamento completo dos tópicos MQTT.
- 🔄 **[STATE_FLOW.md](./docs/STATE_FLOW.md):** Máquinas de estado, fluxo do MFA Óptico (Renderização Sob Demanda) e tratamento de modo degradado.
- 📏 **[CONVENTIONS.md](./docs/CONVENTIONS.md):** Nossas regras de ouro de engenharia.

---

## 🛠️ Stack Tecnológica

| Camada | Tecnologia | Função Principal |
| :--- | :--- | :--- |
| **Frontend Mobile** | Flutter (Dart) | App do cliente e interface web/mobile. |
| **Backend Gateway** | Go (Golang) | API REST de alta concorrência e orquestração de entregas. |
| **Mensageria** | Mosquitto MQTT | Barramento seguro M2M hospedado na nuvem. |
| **Banco de dados** | PostgreSQL | Persistência inicial de catálogo, restaurantes e produtos. |
| **Firmware (Tranca)** | C++ / PlatformIO | ESP32 com display OLED e controle do solenoide. |
| **Navegação (Core)** | ROS 2 / Raspberry Pi | Nó computacional embarcado no robô. |

---

## 📂 Estrutura do Monorepo

```text
app/
├── database/       # Bootstrap e scripts SQL do PostgreSQL
├── gateway/        # Servidor Go (API REST e integração MQTT)
├── mobile/         # Aplicativo Flutter / build web
├── docker-compose.yml
└── README.md
```

---

## 🚀 Execução principal da aplicação completa

O cenário principal sobe banco, backend e frontend web.

Arquivo utilizado: [`docker-compose.yml`](./docker-compose.yml)

A partir da raiz do repositório:

```bash
docker compose -f ./app/docker-compose.yml up --build
```

Para rodar em background:

```bash
docker compose -f ./app/docker-compose.yml up -d --build
```

Para derrubar:

```bash
docker compose -f ./app/docker-compose.yml down
```

### Portas de acesso

- App web: `http://localhost:8081`
- Backend/API: `http://localhost:8080`
- PostgreSQL: `localhost:5432`

---

## 🚀 Execução local por camada

### 1. Banco isolado da camada gateway

Arquivo utilizado: [`gateway/docker-compose.db.yml`](./gateway/docker-compose.db.yml)

```bash
docker compose -f ./app/gateway/docker-compose.db.yml up -d
```

Para derrubar:

```bash
docker compose -f ./app/gateway/docker-compose.db.yml down
```

Porta:

- PostgreSQL: `localhost:5432`

### 2. Backend + banco a partir da camada mobile

Arquivo utilizado: [`mobile/docker-compose.yml`](./mobile/docker-compose.yml)

```bash
docker compose -f ./app/mobile/docker-compose.yml up --build
```

Para rodar em background:

```bash
docker compose -f ./app/mobile/docker-compose.yml up -d --build
```

Para derrubar:

```bash
docker compose -f ./app/mobile/docker-compose.yml down
```

Portas:

- Backend/API: `http://localhost:8080`
- PostgreSQL: `localhost:5432`

### 3. Frontend + banco a partir da camada gateway

Arquivo utilizado: [`gateway/docker-compose.frontend-db.yml`](./gateway/docker-compose.frontend-db.yml)

```bash
docker compose -f ./app/gateway/docker-compose.frontend-db.yml up --build
```

Para rodar em background:

```bash
docker compose -f ./app/gateway/docker-compose.frontend-db.yml up -d --build
```

Para derrubar:

```bash
docker compose -f ./app/gateway/docker-compose.frontend-db.yml down
```

Portas:

- App web: `http://localhost:8081`
- PostgreSQL: `localhost:5432`

### 4. Login mockado do restaurante no app Flutter

O fluxo de restaurante no app está temporariamente acoplado aos dados seed do banco para desenvolvimento e demonstração.

Ao selecionar o perfil `restaurant` no login, o app usa o usuário admin seedado e o restaurante seedado abaixo:

- Usuário admin: `22222222-2222-2222-2222-222222222222`
- Nome do usuário: `Marmitas da Vo Admin`
- E-mail: `admin@marmitasdavo.test`
- Restaurante: `aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1`
- Nome público: `Marmitas da Vo`

Esse vínculo mockado centraliza a identidade do restaurante nas telas de dashboard, perfil e gestão de produtos, evitando depender de `user.id` como se fosse o `restaurant_id`.

---

## 🧪 Execução manual das camadas

### Pré-requisitos locais

Instale localmente:

- Docker e Docker Compose
- Go 1.22+ (para o gateway)
- Flutter SDK 3.32+ (para o app)
- Um dispositivo/emulador Android, iOS ou navegador compatível para o Flutter

### Ordem recomendada para subir sem Docker completo

1. Suba o banco PostgreSQL
2. Rode o backend Go
3. Rode o app Flutter apontando para a API local

### Banco (PostgreSQL via compose)

```bash
docker compose -f ./app/gateway/docker-compose.db.yml up -d
```

Para validar se o banco está ativo:

```bash
docker compose -f ./app/gateway/docker-compose.db.yml ps
```

Porta padrão do PostgreSQL:

- `localhost:5432`

### Backend (Go)

Se existir configuração de ambiente local, crie o arquivo `.env` com base em [`app/.env.example`](./.env.example) e ajuste os valores necessários para o gateway.

Instale dependências e rode o backend:

```bash
cd app/gateway
go mod tidy
go run ./cmd/gateway/main.go
```

Para validar o backend:

```bash
curl http://localhost:8080/health
```

Porta padrão da API:

- `http://localhost:8080`

### Frontend (Flutter)

Instale dependências do Flutter:

```bash
cd app/mobile
flutter pub get
```

Executar no Chrome/Web apontando para a API local:

```bash
flutter run -d chrome --dart-define=API_BASE_URL=http://localhost:8080
```

Executar em emulador Android local:

```bash
flutter run --dart-define=API_BASE_URL=http://10.0.2.2:8080
```

Executar em dispositivo físico na mesma rede:

```bash
flutter run --dart-define=API_BASE_URL=http://SEU_IP_LOCAL:8080
```

Gerar build web localmente:

```bash
flutter build web --release --dart-define=API_BASE_URL=http://localhost:8080
```

Gerar APK de desenvolvimento:

```bash
flutter build apk --dart-define=API_BASE_URL=http://SEU_IP_LOCAL:8080
```

### Comandos úteis de desenvolvimento

Formatar o código Go:

```bash
cd app/gateway && gofmt -w ./...
```

Executar testes do backend:

```bash
cd app/gateway && go test ./...
```

Analisar o app Flutter:

```bash
cd app/mobile && flutter analyze
```

Executar testes Flutter:

```bash
cd app/mobile && flutter test
```

Observações importantes:

- `localhost` funciona para web/desktop local, mas não para Android Emulator; nesse caso use `10.0.2.2`.
- Em dispositivo físico, substitua `SEU_IP_LOCAL` pelo IP da máquina que está rodando o gateway.
- O app móvel usa `API_BASE_URL` em tempo de compilação via `--dart-define`.
- O dashboard de restaurante agora consome pedidos por restaurante via `GET /api/restaurants/{id}/orders`.
- A tela de produtos do restaurante usa integração real com backend para listar, criar, editar e alternar disponibilidade (`is_available`).
- A tela de rastreamento do restaurante continua demonstrativa/simulada até a integração com telemetria persistida do robô.

---

## 🍱 Fluxo atual do restaurante

Estado atual do fluxo de restaurante no frontend:

- Login `restaurant` usa a identidade seedada de `Marmitas da Vo`.
- Dashboard do restaurante lista pedidos pelo `restaurant_id`, não mais pelo `client_user_id`.
- Gestão de produtos usa chamadas reais de API para criação, edição e atualização de disponibilidade.
- Acompanhamento do robô na tela de tracking ainda representa um cenário visual simulado.

## 🎓 Créditos

Desenvolvido como parte do **Projeto Integrador de Tecnologias (PIT)** da Faculdade de Tecnologia (FT) - Engenharia Mecatrônica - Universidade de Brasília (UnB).