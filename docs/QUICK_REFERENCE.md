# BitGossip Quick Reference

## Quick Commands

### Starting the Network
```bash
# Start all seeds
scripts/start_seeds.sh
# Or: make run_seeds

# Start all peers
scripts/start_peers.sh
# Or: make run_peers

# OR: Start individual nodes
./seed <port>
./peer <port>
```

### Monitoring
```bash
# Real-time monitoring (color-coded)
scripts/monitor_logs.sh

# Watch specific node
tail -f output/peer_6001.txt
```

### Testing
```bash
# Run comprehensive test
scripts/test_network.sh
# Or: make test

# Manual dead node test
pkill -9 peer
```

### Cleanup
```bash
# Stop all nodes
scripts/stop_all.sh
# Or: make stop

# Clean log files
scripts/clean_output.sh
# Or: make clean_output
```

## Log Message Types

### Seed Node Messages
- `CONSENSUS REACHED: Peer X registered` - Successful registration
- `CONSENSUS FAILED: Peer X registration rejected` - Failed registration
- `Voted YES for registration` - Vote cast for peer admission
- `Dead node report: X from Y` - Received failure report
- `CONSENSUS REACHED: Peer X removed` - Peer removed from network

### Peer Node Messages
- `Registered with seed X` - Successful seed registration
- `Connected to peer X` - Outgoing connection established
- `Incoming connection from X` - Incoming connection accepted
- `GOSSIP GENERATED: ...` - New message created
- `GOSSIP RECEIVED: ... (from X)` - Message received for first time
- `SUSPECTING peer X` - Peer appears unresponsive
- `PEER CONSENSUS: X confirmed dead` - Multiple peers agree
- `REPORTING to seeds: Dead node X` - Notifying seeds of failure

## Configuration

### config.txt Format
```
<IP>,<Port>
127.0.0.1,5001
127.0.0.1,5002
127.0.0.1,5003
```

### Adding More Nodes

**Add a seed:**
1. Add line to config.txt
2. Restart all seeds: `./stop_all.sh` then `./start_seeds.sh`

**Add a peer:**
1. Just start it: `python3 peer.py <new_port>`
2. Or add to start_peers.sh for automation

## Common Issues

### "Address already in use"
```bash
lsof -ti:<port> | xargs kill -9
# OR
scripts/stop_all.sh
# OR: make stop
```

### Peers not connecting
- Ensure seeds are running first
- Wait 1-2 seconds between starting peers
- Check config.txt is correct

### No consensus reached
- Need at least ⌊n/2⌋ + 1 seeds running
- Check all seeds in config.txt are started
- Verify network connectivity

### Gossip not propagating
- Ensure peers have neighbors (check "Connected to peer" logs)
- Wait for network to stabilize (2-3 seconds)
- Check power-law distribution created connections

## Testing Checklist

- [ ] All seeds start and log "Seed running at..."
- [ ] Peers register with quorum seeds
- [ ] Seeds reach consensus for registration
- [ ] Peers connect to neighbors (power-law distribution)
- [ ] Gossip messages generated (10 per peer)
- [ ] Gossip messages propagate to connected peers
- [ ] No duplicate message processing
- [ ] Periodic liveness checks (ping)
- [ ] Suspicion after 3 failed pings
- [ ] Peer-level consensus on dead nodes
- [ ] Dead node reports sent to seeds
- [ ] Seed-level consensus on removal
- [ ] Removed peers no longer appear in peer lists

## Network Statistics

Use `./analyze_network.py` to see:
- Registration/removal counts per seed
- Neighbor counts per peer (degree distribution)
- Gossip message generation/reception
- Suspicion and dead node reports
- Overall propagation rate

## Performance Tips

- **Faster testing**: Reduce sleep times in peer.py
- **More messages**: Increase `max_messages` in peer.py
- **Longer observation**: Wait 60+ seconds for full propagation
- **Stress test**: Start 10+ peers to test scalability

## Debug Mode

Add print statements to see:
```python
# In peer.py
self.log(f"DEBUG: Neighbor count = {len(self.neighbors)}")
self.log(f"DEBUG: Message hash = {msg_hash}")

# In seed.py
log(f"DEBUG: Current votes = {votes}")
```

## Expected Behavior

1. **Startup (0-5s)**: Seeds start, peers register
2. **Connection (5-10s)**: Peers connect to neighbors
3. **Gossip (10-60s)**: Messages generated and propagated
4. **Liveness (Ongoing)**: Continuous ping checks
5. **Failure**: 10-20s to detect and reach consensus

## File Locations

- **Logs**: `output/seed_<port>.txt`, `output/peer_<port>.txt`
- **Config**: `config.txt`
- **Scripts**: `*.sh` files in project root
