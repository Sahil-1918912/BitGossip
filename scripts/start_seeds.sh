#!/bin/bash

# Compile and start all seed nodes in the background

echo "Compiling seed program..."
make seed

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

echo "Starting seed nodes..."

./seed 5001 &
sleep 0.5

./seed 5002 &
sleep 0.5

./seed 5003 &
sleep 0.5

echo "All seed nodes started!"
echo "Seed logs available in output/ directory"
