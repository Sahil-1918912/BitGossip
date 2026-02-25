#!/bin/bash

# Stop all seed and peer processes

echo "Stopping all nodes..."

pkill -f "python3 seed.py"
pkill -f "python3 peer.py"

echo "All nodes stopped!"
