#!/bin/bash
# restart-containers.sh
# Clean and restart all Podman/Docker containers for the UnBot Delivery project

set -e  # Exit on error

echo "🧹 Cleaning up containers and images..."

# Stop and remove all containers from docker-compose
echo "📦 Stopping containers..."
podman compose down 2>/dev/null || docker-compose down 2>/dev/null || true

# Remove all stopped containers
echo "🗑️  Removing stopped containers..."
podman container prune -f 2>/dev/null || docker container prune -f 2>/dev/null || true

# Remove unused images
echo "🖼️  Removing unused images..."
podman image prune -f 2>/dev/null || docker image prune -f 2>/dev/null || true

# Optional: Remove volumes (uncomment if you want fresh database)
# echo "💾 Removing volumes..."
# podman volume prune -f 2>/dev/null || docker volume prune -f 2>/dev/null || true

echo ""
echo "🔨 Building containers without cache..."
podman compose build --no-cache 2>/dev/null || docker-compose build --no-cache 2>/dev/null

echo ""
echo "🚀 Starting containers..."
podman compose up -d 2>/dev/null || docker-compose up -d 2>/dev/null

echo ""
echo "✅ Done! Containers are starting up..."
echo ""
echo "📊 Container status:"
podman compose ps 2>/dev/null || docker-compose ps 2>/dev/null

echo ""
echo "📝 View logs with:"
echo "   podman compose logs -f"
echo "   or"
echo "   docker-compose logs -f"
echo ""
echo "🌐 Access points:"
echo "   - Mobile Web: http://localhost:8081"
echo "   - API Gateway: http://localhost:8080"
echo "   - PostgreSQL: localhost:5432"
