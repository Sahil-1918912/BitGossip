#!/bin/bash

# Stop all seed and peer processes

echo "Stopping all nodes..."

pkill -9 seed
pkill -9 peer

echo "All nodes stopped!"
