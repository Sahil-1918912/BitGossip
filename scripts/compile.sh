#!/bin/bash

# Compile all programs

echo "Compiling BitGossip Network..."
echo ""

make all

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Compilation successful!"
    echo "  - seed: Seed node program"
    echo "  - peer: Peer node program"
    echo ""
    echo "You can now run:"
    echo "  ./start_seeds.sh  - Start seed nodes"
    echo "  ./start_peers.sh  - Start peer nodes"
    echo "  ./stop_all.sh     - Stop all nodes"
else
    echo ""
    echo "✗ Compilation failed!"
    exit 1
fi
