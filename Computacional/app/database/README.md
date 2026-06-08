# Database - primeira fase de implementação

Este diretório concentra os artefatos de banco de dados usados na primeira fase de implementação do backend do projeto UnBot Delivery.

Nesta fase, o objetivo do banco é dar suporte ao **catálogo inicial da aplicação**, com foco em leitura e navegação de dados pelo app, sem ainda incluir pedidos, OTP, comandos do robô ou telemetria.

## Escopo da fase 1

A fase 1 implementa somente a base necessária para os primeiros endpoints de catálogo:

- usuários
- restaurantes
- produtos
- dados de seed para desenvolvimento local

Os arquivos atualmente usados no bootstrap mínimo são:

- [`001_extensions.up.sql`](app/database/001_extensions.up.sql)
- [`010_users.up.sql`](app/database/010_users.up.sql)
- [`020_restaurants.up.sql`](app/database/020_restaurants.up.sql)
- [`030_products.up.sql`](app/database/030_products.up.sql)
- [`001_sample_data.sql`](app/database/seeds/001_sample_data.sql)

## Objetivo funcional desta fase

O banco, nesta etapa, existe para suportar principalmente:

- listagem de restaurantes
- consulta de restaurante por identificador
- listagem de produtos por restaurante
- substituição gradual dos dados mockados do app por dados persistidos

Isso permite iniciar o backend com baixo acoplamento e baixo impacto sobre os demais serviços do ecossistema.

## O que ainda não faz parte desta fase

As estruturas abaixo já podem existir como migrations futuras no repositório, mas **não fazem parte do bootstrap atual da fase 1**:

- pedidos
- itens de pedido
- OTP de retirada
- comandos enviados ao robô
- telemetria do robô

Essas features devem ser incorporadas somente quando a próxima fase do backend começar.

## Ordem das migrations da fase 1

A ordem atual de inicialização foi definida para respeitar dependências entre extensões, tipos e tabelas:

1. [`001_extensions.up.sql`](app/database/001_extensions.up.sql)
2. [`010_users.up.sql`](app/database/010_users.up.sql)
3. [`020_restaurants.up.sql`](app/database/020_restaurants.up.sql)
4. [`030_products.up.sql`](app/database/030_products.up.sql)
5. [`001_sample_data.sql`](app/database/seeds/001_sample_data.sql)

## Estrutura da modelagem inicial

### [`users`](app/database/010_users.up.sql)

Representa os usuários do sistema.

Campos principais:

- `id`
- `role`
- `name`
- `email`
- `phone`
- `default_address`
- `profile_image_url`
- `created_at`
- `updated_at`
- `deleted_at`

Nesta fase, a tabela permite sustentar usuários clientes e também responsáveis por restaurante.

### [`restaurants`](app/database/020_restaurants.up.sql)

Representa os restaurantes visíveis no catálogo.

Campos principais:

- `id`
- `owner_user_id`
- `name`
- `emoji`
- `bg_color`
- `rating`
- `eta_minutes`
- `is_open`
- `created_at`
- `updated_at`
- `deleted_at`

A relação com [`users`](app/database/010_users.up.sql) é feita por `owner_user_id`.

### [`products`](app/database/030_products.up.sql)

Representa os produtos disponíveis em cada restaurante.

Campos principais:

- `id`
- `restaurant_id`
- `name`
- `description`
- `emoji`
- `price_cents`
- `is_available`
- `sort_order`
- `created_at`
- `updated_at`
- `deleted_at`

A relação com [`restaurants`](app/database/020_restaurants.up.sql) é feita por `restaurant_id`.

## Seed de desenvolvimento

O arquivo [`001_sample_data.sql`](app/database/seeds/001_sample_data.sql) popula o ambiente local com dados mínimos para testes.

Ele inclui exemplos de:

- usuário cliente
- usuário administrador de restaurante
- restaurantes
- produtos

Esse seed existe para acelerar validações locais no app e no backend, sem depender de cadastros manuais.

## Relação com o ambiente local

O bootstrap mínimo da fase 1 está refletido em:

- [`gateway/docker-compose.yml`](app/gateway/docker-compose.yml)
- [`README.md`](app/README.md)

Isso significa que o ambiente local do banco está propositalmente restrito ao necessário para a primeira feature real do backend.

## Como executar no ambiente atual

Hoje, a forma principal de subir o banco para esta fase é usando [`docker-compose.yml`](app/gateway/docker-compose.yml).

### Subir o PostgreSQL via Docker Compose

A partir da raiz do repositório:

```bash
docker compose -f ./app/gateway/docker-compose.yml up -d
```

Para derrubar o ambiente:

```bash
docker compose -f ./app/gateway/docker-compose.yml down
```

### Porta de acesso

- PostgreSQL: `localhost:5432`

### Cenários em que o banco também sobe

O PostgreSQL também é inicializado nos cenários abaixo:

- [`docker-compose.yml`](app/docker-compose.yml)
- [`mobile/docker-compose.yml`](app/mobile/docker-compose.yml)
- [`gateway/docker-compose.frontend-db.yml`](app/gateway/docker-compose.frontend-db.yml)

## Reset do banco local

Se quiser recriar o banco do zero e reaplicar os scripts de inicialização, derrube o ambiente e remova o volume associado.

Exemplo:

```bash
docker compose -f ./app/gateway/docker-compose.yml down -v
```

Depois disso, suba novamente:

```bash
docker compose -f ./app/gateway/docker-compose.yml up -d
```

## Critério de conclusão da fase 1 do banco

A primeira fase do banco pode ser considerada pronta quando:

- o ambiente local sobe com o bootstrap mínimo definido
- os dados de seed são carregados com sucesso
- o backend consegue consultar restaurantes e produtos
- o app consegue começar a substituir dados mockados por dados vindos do backend
- ainda não existe dependência obrigatória de MQTT, ESP32, robô ou telemetria

## Próxima evolução esperada

Depois da estabilização desta fase, a próxima evolução natural do banco tende a incluir:

- pedidos
- itens de pedido
- OTP de retirada
- comandos para integração com gateway/robô
- telemetria operacional

Essas adições devem continuar respeitando o isolamento por feature e a ativação gradual conforme a necessidade real do backend.
