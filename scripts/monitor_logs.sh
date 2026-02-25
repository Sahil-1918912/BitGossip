#!/bin/bash

# Monitor all logs in real-time with color coding

# Check if output directory exists
if [ ! -d "output" ]; then
    echo "No output directory found. Start the network first."
    exit 1
fi

# Count seed and peer files
SEED_COUNT=$(ls output/seed_*.txt 2>/dev/null | wc -l)
PEER_COUNT=$(ls output/peer_*.txt 2>/dev/null | wc -l)

echo "========================================="
echo "Monitoring BitGossip Network Logs"
echo "  Seeds: $SEED_COUNT"
echo "  Peers: $PEER_COUNT"
echo "========================================="
echo ""
echo "Press Ctrl+C to stop monitoring"
echo ""

# Monitor all log files
tail -f output/*.txt 2>/dev/null | while IFS= read -r line; do
    # Color code different message types
    if echo "$line" | grep -q "CONSENSUS REACHED"; then
        echo -e "\033[1;32m$line\033[0m"  # Green for consensus
    elif echo "$line" | grep -q "GOSSIP"; then
        echo -e "\033[1;34m$line\033[0m"  # Blue for gossip
    elif echo "$line" | grep -q "Dead\|SUSPECTING\|failed"; then
        echo -e "\033[1;31m$line\033[0m"  # Red for failures
    elif echo "$line" | grep -q "REGISTERED\|Connected"; then
        echo -e "\033[1;33m$line\033[0m"  # Yellow for connections
    else
        echo "$line"
    fi
done
