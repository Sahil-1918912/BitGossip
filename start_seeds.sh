#!/bin/bash

# Start all seed nodes in the background

echo "Starting seed nodes..."

python3 seed.py 5001 &
sleep 0.5

python3 seed.py 5002 &
sleep 0.5

python3 seed.py 5003 &
sleep 0.5

echo "All seed nodes started!"
echo "Seed logs available in output/ directory"
