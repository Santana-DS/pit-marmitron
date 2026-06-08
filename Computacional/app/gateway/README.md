# UnBot Gateway V2.1 (Security & Cloud Ready)

Backend em Go e Gateway MQTT para o sistema de entregas autônomas UnBot Delivery.

## 🌩️ Arquitetura e Nuvem (MFA Óptico e Resiliência)

Na versão 2.0, migramos o Broker Mosquitto para a **Nuvem AWS (EC2)**, eliminando o ponto único de falha (o PC do robô). Agora, na versão **2.1**, o nosso Gateway atua como o cérebro da nova **Autenticação em Duas Etapas por Proximidade Física (MFA Óptico)**.

**Como funciona o novo fluxo:**

1. O Gateway Go gera um OTP único no momento do despacho.
2. Quando o cliente inicia a etapa de leitura no app, o backend publica o comando (`robot/commands/display_qr`) para o hardware (ESP32) desenhar o QR Code dinâmico no display OLED sob demanda.
3. O cliente escaneia o código via app; o Gateway valida o pedido via `POST /api/validate-code` e, em caso de sucesso, publica o comando de abertura (`robot/commands/unlock`) diretamente para a tranca.

## 📂 Estrutura de Diretórios

```text
gateway/
├── cmd/
│   └── gateway/
│       └── main.go
├── internal/
│   ├── api/
│   ├── catalog/
│   ├── config/
│   ├── database/
│   ├── mqtt/
│   └── services/
├── scripts/
├── Dockerfile
├── docker-compose.yml
├── docker-compose.frontend-db.yml
├── go.mod
└── README.md
```

## 🚀 Execução da camada gateway

Esta camada possui dois cenários principais de execução com Compose.

### 1. Banco isolado da camada gateway

Arquivo utilizado: [`docker-compose.db.yml`](app/gateway/docker-compose.db.yml)

A partir da raiz do repositório:

```bash
docker compose -f ./app/gateway/docker-compose.db.yml up -d
```

Para derrubar:

```bash
docker compose -f ./app/gateway/docker-compose.db.yml down
```

Porta exposta:

- PostgreSQL: `localhost:5432`

### 2. Frontend + banco a partir da camada gateway

Arquivo utilizado: [`docker-compose.frontend-db.yml`](app/gateway/docker-compose.frontend-db.yml)

A partir da raiz do repositório:

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

Portas expostas:

- App web: `http://localhost:8081`
- PostgreSQL: `localhost:5432`

## 🚀 Execução manual do gateway

Se quiser executar apenas o backend localmente, use o banco em compose e rode o binário Go fora de container.

### Preparação

```bash
cd app/gateway
go mod tidy
```

Se houver arquivo de ambiente, configure-o conforme o projeto utilizar.

### Subir apenas o banco

```bash
docker compose -f ./app/gateway/docker-compose.db.yml up -d
```

### Rodar o backend localmente

```bash
cd app/gateway
go run ./cmd/gateway/main.go
```

Porta padrão da API:

- `http://localhost:8080`

## 🌐 Contratos principais da API

A API expõe rotas HTTP para o aplicativo Flutter e orquestra comandos via MQTT para o hardware.

- `GET /api/restaurants`
  - Função: lista restaurantes persistidos no PostgreSQL.
- `GET /api/restaurants/{id}`
  - Função: retorna detalhes de um restaurante por identificador.
- `GET /api/restaurants/{id}/products`
  - Função: lista os produtos de um restaurante.
- `GET /api/restaurants/{id}/orders`
  - Função: lista pedidos escopados por restaurante para o dashboard operacional.
- `POST /api/restaurants/{id}/products`
  - Função: cria um novo produto para o restaurante.
- `PUT /api/restaurants/{id}/products/{productId}`
  - Função: atualiza dados do produto, incluindo disponibilidade (`is_available`).
- `POST /api/orders/{id}/dispatch`
  - Função: orquestra a entrega e publica o comando de navegação.
- `POST /api/orders/{id}/wake-display`
  - Função: renderiza o QR dinamicamente no display do robô sob demanda do app.
- `POST /api/validate-code`
  - Função: valida o OTP/QR e publica o comando de abertura da trava.

## 📡 Comunicação MQTT

Utilize o programa MQTT Explorer com os dados abaixo para auditar os tópicos quando estiver usando o broker em nuvem:

- Host: `3.22.171.3`
- Port: `1883`
- Username: `gateway`
- Password: solicitar ao responsável do ambiente

Exemplo de teste HTTP local:

```bash
curl -X POST http://localhost:8080/api/validate-code \
  -H "Content-Type: application/json" \
  -d '{"code":"1234","order_id":"order_mock_001"}'
```

## 📊 Estado Atual (Kanban de Sprints)

| Sprint | Foco | Status |
| :--- | :--- | :--- |
| **V2.0** | Migração Go, Nuvem MQTT, Validação e Dispatch HTTP | ✅ Concluído |
| **V2.1** | Integração Flutter, MFA físico, UI State e firmware ESP32 | ✅ Concluído |
| **V2.2** | Persistência inicial em PostgreSQL e leitura de catálogo | 🟡 Em andamento |
| **V3.0** | WebRTC Raspberry Pi, joystick mobile e transmissão de vídeo | ⏳ Na fila |
