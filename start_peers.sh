#!/bin/bash

# Start peer nodes in the background
# You can add more peers as needed

echo "Starting peer nodes..."

python3 peer.py 6001 &
sleep 1

python3 peer.py 6002 &
sleep 1

python3 peer.py 6003 &
sleep 1

python3 peer.py 6004 &
sleep 1

python3 peer.py 6005 &
sleep 1

echo "All peer nodes started!"
echo "Peer logs available in output/ directory"
