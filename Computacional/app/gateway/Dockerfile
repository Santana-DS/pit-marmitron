FROM golang:1.25-alpine AS builder

WORKDIR /src

COPY go.mod go.sum ./
RUN go mod download

COPY . ./
RUN CGO_ENABLED=0 GOOS=linux go build -o /bin/unbot-gateway ./cmd/gateway

FROM alpine:3.20

WORKDIR /app

RUN adduser -D -H -u 10001 appuser

COPY --from=builder /bin/unbot-gateway /app/unbot-gateway

USER appuser

EXPOSE 8080

ENTRYPOINT ["/app/unbot-gateway"]
