# UnBot Delivery — Frontend (Flutter App)

Aplicativo mobile do cliente para o ecossistema UnBot Delivery. Construído em Flutter, o app é responsável por orquestrar a jornada do utilizador desde o pedido até à autenticação de múltiplos fatores no momento da retirada.

## 📱 Funcionalidades principais

- Gestão de estado reativa para atualização da interface.
- Consumo da API do gateway para catálogo de restaurantes e produtos.
- Fluxo de retirada com OTP/QR code.
- Configuração da URL da API por variável de compilação.

## 🛠️ Tecnologias e pacotes críticos

- [`Flutter`](app/mobile/pubspec.yaml)
- `mobile_scanner`
- `google_fonts`

## 🚀 Execução da camada mobile

A camada mobile possui build web via container e execução Flutter local para desenvolvimento.

### 1. Backend + banco a partir da camada mobile

Arquivo utilizado: [`docker-compose.yml`](app/mobile/docker-compose.yml)

A partir da raiz do repositório:

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

Portas expostas:

- Backend/API: `http://localhost:8080`
- PostgreSQL: `localhost:5432`

> Este cenário sobe o backend e o banco para suportar o desenvolvimento da aplicação, mas não sobe o frontend web.

### Login mockado de restaurante

No estado atual do app, ao escolher o perfil `restaurant` no login, a sessão é vinculada aos dados seedados do restaurante de demonstração:

- Usuário admin: `22222222-2222-2222-2222-222222222222`
- Nome: `Marmitas da Vo Admin`
- E-mail: `admin@marmitasdavo.test`
- Restaurante: `aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1`
- Nome público: `Marmitas da Vo`

Isso garante consistência entre dashboard, perfil e gestão de produtos durante o desenvolvimento, mesmo antes de existir autenticação real para restaurante.

### 2. Execução Flutter local

A partir de [`app/mobile`](app/mobile):

```bash
flutter pub get
flutter run --dart-define=API_BASE_URL=http://localhost:8080
```

Esse comando executa o app apontando para o backend local na porta `8080`.

### 3. Build web em container

O Dockerfile da camada mobile gera a versão web e serve o conteúdo via nginx quando usado nos cenários full ou frontend + banco.

Arquivo relacionado: [`Dockerfile`](app/mobile/Dockerfile)

Nos cenários que sobem o frontend web, a porta exposta é:

- App web: `http://localhost:8081`

## 🔐 Permissões nativas

Para o scanner funcionar corretamente em dispositivos reais, as permissões de hardware devem permanecer configuradas nos manifestos nativos:

- Android: permissão de câmera no `AndroidManifest.xml`
- iOS: `NSCameraUsageDescription` no `Info.plist`

## 📂 Estrutura de rotas e telas

O fluxo principal do app hoje passa por telas como:

- [`client_home_screen.dart`](app/mobile/lib/screens/client/client_home_screen.dart)
- [`order_screen.dart`](app/mobile/lib/screens/client/order_screen.dart)
- [`code_screen.dart`](app/mobile/lib/screens/client/code_screen.dart)
- [`otp_unlock_screen.dart`](app/mobile/lib/screens/client/otp_unlock_screen.dart)
- [`restaurant_home_screen.dart`](app/mobile/lib/screens/restaurant/restaurant_home_screen.dart)
- [`restaurant_products_screen.dart`](app/mobile/lib/screens/restaurant/restaurant_products_screen.dart)
- [`restaurant_tracking_screen.dart`](app/mobile/lib/screens/restaurant/restaurant_tracking_screen.dart)

### Estado atual do fluxo de restaurante

- O dashboard do restaurante carrega pedidos pelo endpoint `GET /api/restaurants/{id}/orders`.
- A gestão de produtos usa integração real com backend para listar, criar, editar e atualizar disponibilidade.
- A tela de rastreamento do restaurante ainda é visual/simulada e não consome telemetria persistida do robô.

## 🌐 Endpoint esperado no desenvolvimento local

Durante o desenvolvimento local, o app deve consumir a API do gateway em:

- `http://localhost:8080`

Ao executar em dispositivo físico ou em outro host, ajuste o valor de `API_BASE_URL` conforme o endereço acessível do backend.
