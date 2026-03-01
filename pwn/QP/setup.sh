#!/bin/bash

echo "================================================"
echo "Quantum Printer - Challenge Setup"
echo "================================================"
echo ""

# Check if running as root for Docker
if [ "$EUID" -eq 0 ]; then
    echo "[!] Warning: Running as root"
fi

# Step 1: Set the flag
echo "[*] Step 1: Configure flag"
read -p "Enter flag (or press Enter for default): " FLAG
if [ -z "$FLAG" ]; then
    FLAG="shellmates{qu4ntum_3nt4ngl3m3nt_1s_d4ng3r0us}"
    echo "[*] Using default flag: $FLAG"
else
    echo "[*] Using custom flag: $FLAG"
fi

echo "$FLAG" > flag.txt
echo "[+] Flag written to flag.txt"
echo ""

# Step 2: Build the binary
echo "[*] Step 2: Build challenge binary"
if [ ! -f "Makefile" ]; then
    echo "[-] Error: Makefile not found"
    exit 1
fi

make clean > /dev/null 2>&1
make release

if [ $? -ne 0 ]; then
    echo "[-] Build failed"
    exit 1
fi

echo "[+] Binary built successfully"
echo ""

# Step 3: Test the binary
echo "[*] Step 3: Testing binary"
if [ ! -f "quantum_printer" ]; then
    echo "[-] Error: quantum_printer binary not found"
    exit 1
fi

echo "5" | timeout 2 ./quantum_printer > /dev/null 2>&1
if [ $? -eq 124 ] || [ $? -eq 0 ]; then
    echo "[+] Binary runs successfully"
else
    echo "[-] Binary test failed"
    exit 1
fi
echo ""

# Step 4: Docker setup
echo "[*] Step 4: Docker deployment"
read -p "Deploy with Docker? (y/n): " DEPLOY
if [ "$DEPLOY" = "y" ] || [ "$DEPLOY" = "Y" ]; then
    if ! command -v docker &> /dev/null; then
        echo "[-] Docker not found. Install Docker first."
        exit 1
    fi
    
    if ! command -v docker-compose &> /dev/null; then
        echo "[-] docker-compose not found. Install docker-compose first."
        exit 1
    fi
    
    cd deployment
    echo "[*] Building Docker image..."
    docker-compose build
    
    if [ $? -ne 0 ]; then
        echo "[-] Docker build failed"
        exit 1
    fi
    
    echo "[+] Docker image built successfully"
    echo ""
    echo "[*] Starting service..."
    docker-compose up -d
    
    if [ $? -ne 0 ]; then
        echo "[-] Failed to start service"
        exit 1
    fi
    
    echo "[+] Service started on port 1337"
    echo ""
    
    # Test connection
    sleep 2
    echo "[*] Testing connection..."
    echo "5" | timeout 2 nc localhost 1337 > /dev/null 2>&1
    if [ $? -eq 0 ] || [ $? -eq 124 ]; then
        echo "[+] Service is responding"
    else
        echo "[!] Service might not be responding yet"
    fi
    
    cd ..
fi

echo ""
echo "================================================"
echo "Setup Complete!"
echo "================================================"
echo ""
echo "Challenge files ready:"
echo "  - quantum_printer (binary)"
echo "  - quantum_printer_v2.c (source)"
echo "  - flag.txt (flag file)"
echo ""
if [ "$DEPLOY" = "y" ] || [ "$DEPLOY" = "Y" ]; then
    echo "Service running at:"
    echo "  nc localhost 1337"
    echo ""
    echo "To stop: cd deployment && docker-compose down"
fi
echo ""
echo "Distribute to players:"
echo "  - quantum_printer (binary)"
echo "  - quantum_printer_v2.c (source)"
echo ""
echo "Keep secret:"
echo "  - flag.txt"
echo "  - exploit.py"
echo "  - WRITEUP.md"
echo ""
