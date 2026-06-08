FROM ghcr.io/cirruslabs/flutter:3.32.0 AS builder

WORKDIR /app

COPY pubspec.yaml pubspec.lock ./
RUN flutter pub get

COPY . ./
RUN flutter build web --release --dart-define=API_BASE_URL=http://localhost:8080

FROM nginx:1.27.4-alpine

COPY --from=builder /app/build/web /usr/share/nginx/html

EXPOSE 80
