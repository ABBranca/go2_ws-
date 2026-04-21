#!/bin/bash

# Configurazione
ROBOT_USER="unitree"
ROBOT_IP="192.168.123.18"
IMAGE_NAME="go2_nav_stack:latest"
DOCKER_FILE="docker/Dockerfile"

# Colori per l'output
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo -e "${GREEN}--- Inizio deploy dello stack su Unitree Go2 ---${NC}"

# 1. Build per ARM64
echo -e "${GREEN}[1/3] Building dell'immagine Docker (ARM64)...${NC}"
docker buildx build --platform linux/arm64 -t $IMAGE_NAME -f $DOCKER_FILE --load .

if [ $? -ne 0 ]; then
    echo "ERRORE: Build fallito."
    exit 1
fi

# 2. Trasferimento
echo -e "${GREEN}[2/3] Trasferimento immagine al robot ($ROBOT_IP)...${NC}"
docker save $IMAGE_NAME | gzip | ssh -C $ROBOT_USER@$ROBOT_IP 'zcat | docker load'

if [ $? -ne 0 ]; then
    echo "ERRORE: Trasferimento fallito. Controlla la connessione Ethernet (192.168.123.x)."
    exit 1
fi

# 3. Avvio
echo -e "${GREEN}[3/3] Avvio del container sul robot...${NC}"
ssh $ROBOT_USER@$ROBOT_IP << EOF
    docker stop go2_navigation 2>/dev/null || true
    docker rm go2_navigation 2>/dev/null || true

    docker run -d \
      --name go2_navigation \
      --privileged \
      --network host \
      --restart always \
      -e CYCLONEDDS_URI=file:///etc/cyclonedds.xml \
      -v /home/unitree/go2_ws/docker/cyclonedds.xml:/etc/cyclonedds.xml \
      $IMAGE_NAME
EOF

echo -e "${GREEN}--- Deploy completato! ---${NC}"
echo "Usa: ssh $ROBOT_USER@$ROBOT_IP -t 'docker exec -it go2_navigation bash' per entrare."
