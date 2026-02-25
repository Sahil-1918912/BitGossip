#!/bin/bash

# Compile and start peer nodes in the background
# You can add more peers as needed

echo "Compiling peer program..."
make peer

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Starting peer nodes..."

./peer 6001 &
sleep 1

./peer 6002 &
sleep 1

./peer 6003 &
sleep 1

./peer 6004 &
sleep 1

./peer 6005 &
sleep 1

echo "All peer nodes started!"
echo "Peer logs available in output/ directory"
