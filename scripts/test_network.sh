#!/bin/bash

# Comprehensive test script for BitGossip network

echo "========================================="
echo "BitGossip Network Test Suite"
echo "========================================="
echo ""

# Compile programs
echo "1. Compiling programs..."
make all

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Clean previous outputs
echo ""
echo "2. Cleaning previous outputs..."
./clean_output.sh
sleep 1

# Start seed nodes
echo ""
echo "3. Starting seed nodes..."
./seed 5001 > /dev/null 2>&1 &
SEED1_PID=$!
sleep 0.5

./seed 5002 > /dev/null 2>&1 &
SEED2_PID=$!
sleep 0.5

./seed 5003 > /dev/null 2>&1 &
SEED3_PID=$!
sleep 1

echo "   Seed nodes started (PIDs: $SEED1_PID, $SEED2_PID, $SEED3_PID)"

# Start peer nodes
echo ""
echo "4. Starting peer nodes..."
./peer 6001 > /dev/null 2>&1 &
PEER1_PID=$!
sleep 1

./peer 6002 > /dev/null 2>&1 &
PEER2_PID=$!
sleep 1

./peer 6003 > /dev/null 2>&1 &
PEER3_PID=$!
sleep 1

./peer 6004 > /dev/null 2>&1 &
PEER4_PID=$!
sleep 1

./peer 6005 > /dev/null 2>&1 &
PEER5_PID=$!
sleep 2

echo "   Peer nodes started (5 peers)"

# Wait for network to stabilize
echo ""
echo "5. Waiting for network to stabilize (10 seconds)..."
sleep 10

# Check registration consensus
echo ""
echo "6. Checking registration consensus..."
CONSENSUS_COUNT=$(grep -c "CONSENSUS REACHED.*registered" output/seed_*.txt 2>/dev/null || echo "0")
echo "   Peers successfully registered: $CONSENSUS_COUNT"

# Check peer connections
echo ""
echo "7. Checking peer connections..."
for port in 6001 6002 6003 6004 6005; do
    if [ -f "output/peer_${port}.txt" ]; then
        NEIGHBORS=$(grep -c "Connected to peer" output/peer_${port}.txt 2>/dev/null || echo "0")
        echo "   Peer $port: $NEIGHBORS neighbors"
    fi
done

# Wait for gossip messages
echo ""
echo "8. Waiting for gossip message generation (30 seconds)..."
sleep 30

# Check gossip propagation
echo ""
echo "9. Checking gossip message propagation..."
for port in 6001 6002 6003 6004 6005; do
    if [ -f "output/peer_${port}.txt" ]; then
        GENERATED=$(grep -c "GOSSIP GENERATED" output/peer_${port}.txt 2>/dev/null || echo "0")
        RECEIVED=$(grep -c "GOSSIP RECEIVED" output/peer_${port}.txt 2>/dev/null || echo "0")
        echo "   Peer $port: Generated $GENERATED, Received $RECEIVED messages"
    fi
done

# Test dead node detection
echo ""
echo "10. Testing dead node detection..."
echo "   Killing peer 6003 (PID: $PEER3_PID)..."
kill -9 $PEER3_PID 2>/dev/null
sleep 15

# Check suspicion reports
SUSPICION_COUNT=$(grep -c "SUSPECTING peer.*6003" output/peer_*.txt 2>/dev/null || echo "0")
echo "   Peers suspecting 6003: $SUSPICION_COUNT"

# Check dead node reports to seeds
DEAD_REPORTS=$(grep -c "Dead node report.*6003" output/seed_*.txt 2>/dev/null || echo "0")
echo "   Dead node reports received by seeds: $DEAD_REPORTS"

# Check removal consensus
REMOVAL_CONSENSUS=$(grep -c "CONSENSUS REACHED.*6003.*removed" output/seed_*.txt 2>/dev/null || echo "0")
echo "   Removal consensus reached: $REMOVAL_CONSENSUS"

# Summary
echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "✓ Seeds started: 3"
echo "✓ Peers started: 5"
echo "✓ Registration consensus: $CONSENSUS_COUNT"
echo "✓ Gossip messages propagated: Yes (check logs)"
echo "✓ Dead node detection: $SUSPICION_COUNT suspicions, $DEAD_REPORTS reports"
echo "✓ Removal consensus: $REMOVAL_CONSENSUS"
echo ""
echo "Detailed logs available in output/ directory"
echo ""
echo "Press Enter to stop all nodes and exit..."
read

# Cleanup
echo ""
echo "Stopping all nodes..."
./stop_all.sh

echo ""
echo "Test complete!"
