#!/bin/bash

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Scheduler Simulator Build Script${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

BUILD_TYPE="Release"
CLEAN=false
RUN=false
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -r|--run)
            RUN=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning...${NC}"
    rm -rf build
fi

mkdir -p build
cd build

echo -e "${GREEN}Configuring (${BUILD_TYPE})...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

echo -e "${GREEN}Building with ${CORES} cores...${NC}"
cmake --build . -j${CORES}

echo -e "${GREEN}✅ Build complete!${NC}"

if [ "$RUN" = true ]; then
    echo -e "${GREEN}Running simulation...${NC}"
    ./bin/scheduler_sim
fi