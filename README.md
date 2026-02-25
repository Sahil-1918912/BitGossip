# BitGossip - Gossip-based P2P Network with Consensus

A robust peer-to-peer network implementation featuring gossip-based message dissemination, consensus-driven membership management, and two-level liveness detection.

## Features

### 1. **Consensus-Based Membership Management**
- **Seed-Level Consensus**: Peers must receive approval from a quorum of seed nodes (⌊n/2⌋ + 1) to join the network
- **Registration Voting**: Seed nodes exchange votes before admitting new peers
- **Removal Consensus**: Dead nodes are removed only after seed-level agreement

### 2. **Two-Level Liveness Detection**
- **Peer-Level Consensus**: Multiple neighboring peers must confirm a node is unresponsive before reporting
- **Seed-Level Consensus**: Seeds validate dead-node reports before removing from membership
- **Prevents False Positives**: Mitigates malicious or erroneous failure reports

### 3. **Power-Law Topology**
- Neighbor selection follows power-law degree distribution
- Creates natural network hubs (some nodes with many connections, most with few)
- Improves gossip efficiency and network resilience

### 4. **Gossip Protocol**
- Messages propagate efficiently through the network
- Deduplication via hash-based message list
- Each message traverses each link at most once
- Format: `<timestamp>:<IP>:<message#>`

## Architecture

### Seed Nodes
- Bootstrap the network and maintain peer membership
- Exchange registration and removal proposals
- Provide peer lists to joining nodes
- Ensure consensus before membership changes

### Peer Nodes
- Register with quorum of seed nodes
- Self-organize into power-law topology
- Generate and propagate gossip messages
- Detect neighbor failures via ping
- Report dead nodes after peer consensus

## File Structure

```
BitGossip/
├── seed.py              # Seed node implementation
├── peer.py              # Peer node implementation
├── config.txt           # Seed node configuration (IP,Port)
├── output/              # Log files directory
├── start_seeds.sh       # Start all seed nodes
├── start_peers.sh       # Start all peer nodes
├── stop_all.sh          # Stop all nodes
├── clean_output.sh      # Clean log files
└── README.md            # This file
```

## Configuration

### config.txt
The configuration file contains seed node addresses (one per line):
```
127.0.0.1,5001
127.0.0.1,5002
127.0.0.1,5003
```

You can add more seed nodes, but ensure n ≥ 2 for meaningful consensus.

## Usage

### Quick Start

1. **Clean previous logs** (optional):
   ```bash
   ./clean_output.sh
   ```

2. **Start seed nodes**:
   ```bash
   ./start_seeds.sh
   ```
   This starts all seed nodes defined in config.txt.

3. **Start peer nodes**:
   ```bash
   ./start_peers.sh
   ```
   This starts 5 peer nodes. You can edit the script to add more peers.

4. **Monitor logs**:
   ```bash
   # Watch seed logs
   tail -f output/seed_5001.txt
   
   # Watch peer logs
   tail -f output/peer_6001.txt
   ```

5. **Stop all nodes**:
   ```bash
   ./stop_all.sh
   ```

### Manual Operation

#### Start Individual Seed Node
```bash
python3 seed.py <port>
```
Example:
```bash
python3 seed.py 5001
```

#### Start Individual Peer Node
```bash
python3 peer.py <port>
```
Example:
```bash
python3 peer.py 6001
```

## Testing Scenarios

### 1. Basic Network Formation
- Start 3 seeds, then 5 peers
- Verify all peers register with quorum seeds
- Check peer logs for neighbor connections
- Observe gossip message propagation

### 2. Consensus Registration
- Watch seed logs for "CONSENSUS REACHED" messages
- Verify peers only complete registration after quorum votes
- Check that registration votes are exchanged between seeds

### 3. Gossip Dissemination
- Each peer generates 10 messages (5-second intervals)
- Watch peer logs for "GOSSIP RECEIVED" messages
- Verify messages reach all connected peers
- Confirm no duplicate processing

### 4. Liveness Detection
- Start network with multiple peers
- Kill a peer process manually:
  ```bash
  pkill -f "python3 peer.py 6003"
  ```
- Watch neighbor peers detect failure via ping
- Observe "SUSPECTING peer" messages
- Verify peer consensus before reporting to seeds
- Check seed logs for "Dead node report" and consensus

### 5. Power-Law Topology
- Run network with 10+ peers
- Check peer logs for neighbor counts
- Verify varying degrees (some peers with 2-3 neighbors, others with 4-5)
- Most peers should have low degree, few should have high degree

## Log Output

### Seed Node Logs
```
[2026-02-25 10:15:30.123] Seed running at 127.0.0.1:5001 (Quorum: 2/3)
[2026-02-25 10:15:35.456] Registration request from 127.0.0.1:6001
[2026-02-25 10:15:35.789] Proposing registration for peer 127.0.0.1:6001
[2026-02-25 10:15:36.234] Voted YES for registration of 127.0.0.1:6001
[2026-02-25 10:15:36.567] CONSENSUS REACHED: Peer 127.0.0.1:6001 registered (2/3 votes)
[2026-02-25 10:15:40.890] Dead node report: 127.0.0.1:6003 from 127.0.0.1:6002
[2026-02-25 10:15:41.234] CONSENSUS REACHED: Peer 127.0.0.1:6003 removed (2/3 votes)
```

### Peer Node Logs
```
[2026-02-25 10:15:35.123] Peer starting at 127.0.0.1:6001
[2026-02-25 10:15:35.234] Registered with seed 127.0.0.1:5001
[2026-02-25 10:15:35.345] Registered with seed 127.0.0.1:5002
[2026-02-25 10:15:35.456] Successfully registered with 2 seeds
[2026-02-25 10:15:35.567] Received 3 peers from seed 127.0.0.1:5001
[2026-02-25 10:15:35.678] Selected 3 neighbors using power-law distribution
[2026-02-25 10:15:35.789] Connected to peer 127.0.0.1:6002
[2026-02-25 10:15:40.123] GOSSIP GENERATED: 2026-02-25 10:15:40.123:127.0.0.1:0
[2026-02-25 10:15:41.456] GOSSIP RECEIVED: 2026-02-25 10:15:41.456:127.0.0.1:0 (from 127.0.0.1:6002)
[2026-02-25 10:16:05.789] SUSPECTING peer 127.0.0.1:6003 (failed 3 pings)
[2026-02-25 10:16:06.123] PEER CONSENSUS: 127.0.0.1:6003 confirmed dead by 2 peers
[2026-02-25 10:16:06.234] REPORTING to seeds: Dead node 127.0.0.1:6003
```

## Protocol Details

### Message Formats

1. **Registration**: `REGISTER:<peer_ip>:<peer_port>`
2. **Registration Vote**: `VOTE_REGISTER:<peer_ip>:<peer_port>:<proposer_seed>`
3. **Peer List Request**: `GET_PEERS`
4. **Gossip**: `GOSSIP:<timestamp>:<ip>:<msg#>`
5. **Suspicion**: `SUSPECT:<dead_ip>:<dead_port>:<reporter_ip>`
6. **Dead Node Report**: `Dead Node:<ip>:<port>:<timestamp>:<reporter_ip>`

### Consensus Mechanisms

#### Registration Consensus
1. Peer sends REGISTER to multiple seeds
2. First seed proposes registration, votes YES
3. Seed broadcasts VOTE_REGISTER to other seeds
4. Each seed votes independently
5. After collecting votes, if quorum reached, peer added to PL

#### Removal Consensus
1. Peer detects neighbor unresponsiveness (3 failed pings)
2. Peer broadcasts SUSPECT to its neighbors
3. Neighbors independently verify suspicion
4. After neighbor quorum confirms, dead node reported to seeds
5. First seed proposes removal, votes YES
6. Seed broadcasts VOTE_REMOVE to other seeds
7. After seed quorum, peer removed from all PLs

## Security Considerations

### Attack Mitigations

1. **Sybil Attacks**: Consensus prevents single malicious seed from admitting fake peers
2. **False Failure Reports**: Two-level consensus (peer + seed) prevents malicious failure accusations
3. **Denial of Service**: Quorum requirements prevent single-node attacks
4. **Network Partitioning**: Power-law topology with hubs maintains connectivity

### Potential Vulnerabilities

1. **Collusion**: If majority of seeds or peers are malicious, consensus can be subverted
2. **Timing Attacks**: Network delays could cause false suspicions (mitigated by multiple ping attempts)
3. **Eclipse Attacks**: Malicious peers could isolate victims (mitigated by random neighbor selection)

## Requirements

- Python 3.7+
- Linux/Unix environment (for ping command)
- Network connectivity between nodes

## Troubleshooting

### "Address already in use" error
```bash
# Find and kill process using the port
lsof -ti:5001 | xargs kill -9
```

### Seeds not reaching consensus
- Ensure all seeds in config.txt are running
- Check network connectivity
- Verify quorum calculation (n/2 + 1)

### Peers not connecting
- Verify seeds are running first
- Check config.txt has correct seed addresses
- Ensure sufficient time between starting seeds and peers

### Gossip messages not propagating
- Verify peer connections (check logs)
- Ensure message list deduplication is working
- Check network topology has sufficient connectivity

## Development

### Adding More Seed Nodes
1. Edit config.txt to add new seed address
2. Start the new seed: `python3 seed.py <new_port>`
3. Restart existing seeds to load updated config

### Testing on Multiple Machines
1. Update config.txt with actual IP addresses
2. Update SEED_IP and peer IP in code
3. Ensure firewall allows TCP connections
4. Run seeds on designated machines
5. Run peers on any machines with network access

## License

Educational project for Computer Networks course.

## Authors

Sahil Narkhede 
Anshit Agarwal
