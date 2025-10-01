#!/bin/bash

# ESP32 Build, Upload, and Monitor Script
# Usage: ./deploy.sh

set -e  # Exit on any error

# Configuration
FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc"
PORT="/dev/cu.usbmodem13301"
BAUDRATE="115200"
SKETCH_NAME="ESPTimeCast_ESP32.ino"
DATA_DIR="data"
MKLITTLEFS="/Users/fulvio/Library/Arduino15/packages/esp32/tools/mklittlefs/3.0.0-gnu12-dc7f933/mklittlefs"
LITTLEFS_SIZE="1441792"  # Size of LittleFS partition in bytes

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🔨 ESP32 Build & Deploy Script${NC}"
echo "=================================="
echo "Board: $FQBN"
echo "Port: $PORT"
echo "Sketch: $SKETCH_NAME"
echo ""

# Check if sketch file exists
if [ ! -f "$SKETCH_NAME" ]; then
    echo -e "${RED}❌ Error: $SKETCH_NAME not found in current directory${NC}"
    echo "Make sure you're running this script from the directory containing $SKETCH_NAME"
    exit 1
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo -e "${YELLOW}⚠️  Warning: Port $PORT not found${NC}"
    echo "Available ports:"
    ls /dev/cu.* 2>/dev/null || echo "No USB ports found"
    echo ""
    read -p "Continue anyway? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

read -p "Compile sketch? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}📦 Step 1: Compiling sketch...${NC}"
    if arduino-cli compile --fqbn "$FQBN" "$SKETCH_NAME"; then
        echo -e "${GREEN}✅ Compilation successful!${NC}"
    else
        echo -e "${RED}❌ Compilation failed!${NC}"
        exit 1
    fi
else
    echo "Skipping compilation."
fi

echo ""
echo -e "${YELLOW}📤 Step 2: Uploading to device...${NC}"
if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH_NAME"; then
    echo -e "${GREEN}✅ Upload successful!${NC}"
else
    echo -e "${RED}❌ Upload failed!${NC}"
    exit 1
fi

echo ""
echo -e "${YELLOW}💾 Step 3: Checking data folder...${NC}"
if [ -d "$DATA_DIR" ] && [ "$(ls -A $DATA_DIR)" ]; then
    echo "Found data folder with files:"
    ls -la "$DATA_DIR"
    echo ""
    read -p "Upload data folder to ESP32 LittleFS? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}📂 Creating LittleFS image...${NC}"
        if "$MKLITTLEFS" -c "$DATA_DIR" -p 256 -b 4096 -s "$LITTLEFS_SIZE" littlefs.bin; then
            echo -e "${GREEN}✅ LittleFS image created!${NC}"

            echo -e "${YELLOW}⬆️  Uploading LittleFS image...${NC}"
            if python3 -m esptool --chip esp32c3 --port "$PORT" write-flash 0x290000 littlefs.bin; then
                echo -e "${GREEN}✅ Data folder uploaded successfully!${NC}"
                rm littlefs.bin
            else
                echo -e "${RED}❌ Data folder upload failed!${NC}"
                rm -f littlefs.bin
                exit 1
            fi
        else
            echo -e "${RED}❌ Failed to create LittleFS image!${NC}"
            exit 1
        fi
    else
        echo "Skipping data folder upload"
    fi
else
    echo "No data folder found or folder is empty, skipping data upload"
fi

echo ""
echo -e "${YELLOW}📡 Step 4: Starting serial monitor...${NC}"
echo -e "${BLUE}Press CTRL-C to exit monitor${NC}"
echo "=================================="

# Wait a moment for the device to restart
sleep 2

# Start monitoring
arduino-cli monitor -p "$PORT" --config baudrate="$BAUDRATE"