# BitGossip - Gossip-based P2P Network

A robust peer-to-peer network implementation in **C** featuring gossip-based message dissemination, consensus-driven membership management, and two-level liveness detection with POSIX threads and socket programming.

> **Language:** C (C11 standard)  
> **Threading:** POSIX threads (pthreads)  
> **Networking:** BSD sockets (TCP)  
> **Build:** GNU Make

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [Project Structure](#project-structure)
- [Compilation](#compilation)
- [Usage](#usage)
- [Testing](#testing)
- [Protocol Details](#protocol-details)
- [Documentation](#documentation)
- [Deliverables](#deliverables)

---

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

---

## Quick Start

Get the network running in **3 simple steps**:

```bash
# 1. Compile
make all

# 2. Start seeds (in terminal 1)
scripts/start_seeds.sh

# 3. Start peers (wait 2 seconds, then in terminal 2)
scripts/start_peers.sh
```

**Monitor logs:**
```bash
scripts/monitor_logs.sh
```

**Stop everything:**
```bash
scripts/stop_all.sh
# Or: make stop
```

For detailed instructions, see **[docs/QUICK_START.md](docs/QUICK_START.md)**

---

## Project Structure

```
BitGossip/
├── src/
│   ├── seed.c           # Seed node implementation (16 KB)
│   └── peer.c           # Peer node implementation (28 KB)
├── scripts/
│   ├── start_seeds.sh   # Start all seed nodes
│   ├── start_peers.sh   # Start all peer nodes
│   ├── stop_all.sh      # Stop all nodes
│   ├── clean_output.sh  # Clean log files
│   ├── test_network.sh  # Automated test script
│   └── monitor_logs.sh  # Real-time log monitoring
├── docs/
│   ├── ARCHITECTURE.md  # System architecture and design
│   ├── QUICK_START.md   # Quick start guide
│   └── QUICK_REFERENCE.md # Command reference
├── output/              # Log files directory
├── seed                 # Compiled seed executable (generated)
├── peer                 # Compiled peer executable (generated)
├── Makefile             # Build configuration
├── config.txt           # Seed node configuration
└── README.md            # This file
```

**Key Components:**
- **src/** - C source code for seed and peer nodes
- **scripts/** - Shell scripts for running and managing the network
- **docs/** - Comprehensive documentation
- **output/** - Runtime logs (created automatically)
- **Executables** - Compiled binaries in root directory

---

## Compilation

### Requirements
- GCC compiler (or compatible C compiler)
- POSIX-compliant system (Linux/Unix)
- pthread library
- Make utility

### Build Instructions

**Quick compile:**
```bash
make all
```

**Individual compilation:**
```bash
make seed    # Compile seed node only
make peer    # Compile peer node only
```

**Clean build:**
```bash
make clean   # Remove executables and logs
make all     # Rebuild everything
```

This creates two executables in the root directory:
- `seed` - Seed node program
- `peer` - Peer node program

---

## Configuration

Edit **config.txt** to define seed nodes (format: `IP,Port`):
```
127.0.0.1,5001
127.0.0.1,5002
127.0.0.1,5003
```

You can add more seeds (ensure n ≥ 2). **Quorum** = ⌊n/2⌋ + 1

---

## Usage

### Automated Startup (Recommended)

```bash
# Terminal 1: Start all seeds
scripts/start_seeds.sh
# Or: make run_seeds

# Terminal 2: Start all peers (wait 2 seconds)
scripts/start_peers.sh
# Or: make run_peers
```

### Monitor Logs

```bash
# Real-time colored monitoring
scripts/monitor_logs.sh

# Watch specific nodes
tail -f output/seed_5001.txt
tail -f output/peer_6001.txt
```

### Stop Network

```bash
scripts/stop_all.sh
# Or: make stop
```

### Manual Operation

Start individual nodes:
```bash
./seed <port>    # Example: ./seed 5001
./peer <port>    # Example: ./peer 6001
```

**⚠️ Important:** Seeds must be running before starting peers!

---

## Testing

### Run Automated Test Suite
```bash
scripts/test_network.sh
# Or use make
make test
```

This comprehensive test:
- Compiles all programs
- Starts 3 seeds and 5 peers
- Verifies consensus registration
- Checks gossip propagation
- Tests dead node detection
- Validates two-level consensus for removal
- Shows detailed statistics

### Manual Testing Scenarios

#### 1. Basic Network Formation
```bash
# Terminal 1: Start seeds
scripts/start_seeds.sh
# Or: make run_seeds

# Terminal 2: Start peers (wait 2 seconds after seeds)
scripts/start_peers.sh
# Or: make run_peers

# Terminal 3: Monitor
tail -f output/peer_6001.txt
```

#### 2. Test Dead Node Detection
```bash
# After network is running
pkill -9 peer
# Will kill a random peer - watch other peers detect it
```

#### 3. Specific Peer Kill
```bash
# Find peer PID
ps aux | grep "./peer 6003"

# Kill it
kill -9 <PID>

# Watch logs for detection
tail -f output/peer_6001.txt output/peer_6002.txt
```

## Log Output

### Seed Node Logs (`output/seed_<port>.txt`)
```
[2026-02-25 10:15:30.123] Seed running at 127.0.0.1:5001 (Quorum: 2/3)
[2026-02-25 10:15:35.456] Registration request from 127.0.0.1:6001
[2026-02-25 10:15:35.789] Proposing registration for peer 127.0.0.1:6001
[2026-02-25 10:15:36.234] Voted YES for registration of 127.0.0.1:6001
[2026-02-25 10:15:36.567] CONSENSUS REACHED: Peer 127.0.0.1:6001 registered (2/3 votes)
[2026-02-25 10:15:40.890] Dead node report: 127.0.0.1:6003 from 127.0.0.1:6002
[2026-02-25 10:15:41.234] CONSENSUS REACHED: Peer 127.0.0.1:6003 removed (2/3 votes)
```

### Peer Node Logs (`output/peer_<port>.txt`)
```
[2026-02-25 10:15:35.123] Peer starting at 127.0.0.1:6001
[2026-02-25 10:15:35.234] Registered with seed 127.0.0.1:5001
[2026-02-25 10:15:35.345] Successfully registered with 2 seeds
[2026-02-25 10:15:35.567] Received peers from seed 127.0.0.1:5001
[2026-02-25 10:15:35.678] Selected 3 neighbors using power-law distribution
[2026-02-25 10:15:35.789] Connected to peer 127.0.0.1:6002
[2026-02-25 10:15:40.123] GOSSIP GENERATED: 2026-02-25 10:15:40.123:127.0.0.1:0
[2026-02-25 10:15:41.456] GOSSIP RECEIVED: 2026-02-25 10:15:41.456:127.0.0.1:0 (from 127.0.0.1)
[2026-02-25 10:16:05.789] SUSPECTING peer 127.0.0.1:6003 (failed 3 pings)
[2026-02-25 10:16:06.123] PEER CONSENSUS: 127.0.0.1:6003 confirmed dead by 2 peers
[2026-02-25 10:16:06.234] REPORTING to seeds: Dead node 127.0.0.1:6003
```

## Protocol Details

### Message Formats

1. **Registration**: `REGISTER:<peer_ip>:<peer_port>`
2. **Registration Vote**: `VOTE_REGISTER:<peer_ip>:<peer_port>:<proposer_seed>`
3. **Peer List Request**: `GET_PEERS`
4. **Peer List Response**: `[["ip",port],["ip",port],...]` (JSON-like format)
5. **Gossip**: `GOSSIP:<timestamp>:<ip>:<msg#>`
6. **Suspicion**: `SUSPECT:<dead_ip>:<dead_port>:<reporter_ip>`
7. **Dead Node Report**: `Dead Node:<ip>:<port>:<timestamp>:<reporter_ip>`
8. **Hello**: `HELLO:<ip>:<port>` (peer connection initiation)

### Consensus Mechanisms

#### Registration Consensus
1. Peer sends REGISTER to multiple seeds
2. First seed proposes registration, votes YES
3. Seed broadcasts VOTE_REGISTER to other seeds
4. Each seed votes independently
5. After collecting votes, if quorum reached → peer added to PL

#### Removal Consensus (Two-Level)

**Peer Level:**
1. Peer detects neighbor unresponsiveness (3 failed pings using system `ping`)
2. Peer broadcasts SUSPECT to its neighbors
3. Neighbors independently verify suspicion
4. After neighbor quorum confirms → dead node reported to seeds

**Seed Level:**
1. First seed receives Dead Node report
2. Proposes removal, votes YES
3. Seed broadcasts VOTE_REMOVE to other seeds
4. After seed quorum → peer removed from all PLs

## Development

### Code Structure

#### seed.c
- Multithreaded TCP server
- Handles peer registration, peer list requests, consensus voting
- Thread-safe peer list with mutex protection
- Vote tracking for registration and removal
- Background threads for handling each client connection

#### peer.c
- Multithreaded TCP client/server
- Server thread accepts incoming peer connections
- Client threads connect to neighbors
- Listener threads for each neighbor connection
- Gossip generation thread (10 messages, 5-second intervals)
- Liveness checking thread (ping every 3 seconds)
- Thread-safe data structures (neighbor list, message list, suspicion records)

### Key Data Structures

**Seed Node:**
```c
typedef struct {
    Node peers[MAX_PEERS];
    int count;
    pthread_mutex_t lock;
} PeerList;

typedef struct {
    char peer_id[32];
    int votes[MAX_SEEDS];
    int vote_count;
} VoteRecord;
```

**Peer Node:**
```c
typedef struct {
    char hash[65];
    char message[256];
    char sender_ip[16];
} MessageRecord;

typedef struct {
    char peer_id[32];
    char reporters[MAX_NEIGHBORS][16];
    int reporter_count;
} DeadReport;
```

### Adding More Nodes

**Add a seed:**
1. Edit config.txt to add new seed address
2. Recompile if needed: `make seed`
3. Start the new seed: `./seed <new_port>`
4. Restart existing seeds to load updated config

**Add a peer:**
1. Just start it: `./peer <new_port>`
2. Or add to `scripts/start_peers.sh` for automation

### Testing on Multiple Machines

1. Update config.txt with actual IP addresses of seed machines
2. Update `SEED_IP` and `peer_ip` in source code if needed
3. Recompile on each machine: `make all`
4. Ensure firewall allows TCP connections on seed/peer ports
5. Run seeds on designated machines
6. Run peers on any machines with network access to seeds

## Troubleshooting

### Compilation Errors

**Missing pthread library:**
```bash
sudo apt-get install build-essential
```

**Missing math library:**
Already included in Makefile with `-lm` flag

### Runtime Issues

**"Address already in use" error:**
```bash
# Find process using the port
lsof -ti:5001

# Kill it
kill -9 $(lsof -ti:5001)

# Or stop all
scripts/stop_all.sh
# Or: make stop
```

**Seeds not reaching consensus:**
- Ensure all seeds in config.txt are running
- Check network connectivity: `netstat -tulpn | grep seed`
- Verify quorum calculation (n/2 + 1)
- Check seed logs for vote messages

**Peers not connecting:**
- Verify seeds are running first: `ps aux | grep seed`
- Check config.txt has correct seed addresses
- Ensure sufficient time between starting seeds and peers (2-3 seconds)
- Check peer logs for registration messages

**Segmentation faults:**
- Usually caused by buffer overflows - check if MAX_PEERS/MAX_NEIGHBORS limits exceeded
- Increase array sizes in source code if needed
- Recompile after changes

**Gossip messages not propagating:**
- Verify peer connections: `grep "Connected to peer" output/peer_*.txt`
- Check network topology has sufficient connectivity
- Ensure message list deduplication is working
- Verify timestamps in gossip messages

## Performance Considerations

### Resource Limits
- Maximum peers: 100 (configurable via MAX_PEERS)
- Maximum neighbors per peer: 20 (configurable via MAX_NEIGHBORS)
- Maximum gossip messages: 1000 (configurable via MAX_MESSAGES)
- Maximum seeds: 10 (configurable via MAX_SEEDS)

### Tuning Parameters

Edit source code constants:
```c
#define MAX_GOSSIP_MESSAGES 10    // Messages per peer
#define GOSSIP_INTERVAL 5         // Seconds between messages
#define LIVENESS_INTERVAL 3       // Seconds between ping checks  
#define SUSPICION_THRESHOLD 3     // Failed pings before suspicion
```

After editing, recompile:
```bash
make clean && make all
```

## Security Considerations

### Attack Mitigations
1. **Sybil Attacks**: Consensus prevents single malicious seed from admitting fake peers
2. **False Failure Reports**: Two-level consensus (peer + seed) prevents malicious accusations
3. **Denial of Service**: Quorum requirements prevent single-node attacks
4. **Network Partitioning**: Power-law topology with hubs maintains connectivity

### Potential Vulnerabilities
1. **Collusion**: If majority of seeds or peers are malicious, consensus can be subverted
2. **Buffer Overflows**: Fixed-size buffers could overflow with malicious input
3. **Thread Safety**: Race conditions possible if mutexes not properly used
4. **Resource Exhaustion**: No rate limiting on connections or messages

## Deliverables

As per assignment requirements:

1. **src/seed.c** - Seed node source code
2. **src/peer.c** - Peer node source code  
3. **config.txt** - Configuration file
4. **output/seed_<port>.txt** - Seed output files
5. **output/peer_<port>.txt** - Peer output files
6. **Makefile** - Build configuration

## Project Structure

```
BitGossip/
├── src/
│   ├── seed.c           # Seed node implementation (16 KB)
│   └── peer.c           # Peer node implementation (28 KB)
├── scripts/
│   ├── start_seeds.sh   # Start all seed nodes
│   ├── start_peers.sh   # Start all peer nodes
│   ├── stop_all.sh      # Stop all nodes
│   ├── clean_output.sh  # Clean log files
│   ├── test_network.sh  # Automated test script
│   └── monitor_logs.sh  # Real-time log monitoring
├── docs/
│   ├── ARCHITECTURE.md  # System architecture and design
│   ├── QUICK_START.md   # Quick start guide
│   └── QUICK_REFERENCE.md # Command reference
├── output/              # Log files directory
├── seed                 # Compiled seed executable (generated)
├── peer                 # Compiled peer executable (generated)
├── Makefile             # Build configuration
├── config.txt           # Seed node configuration
└── README.md            # This file
```

## Documentation

- **README.md** (this file) - Complete implementation guide
- **docs/QUICK_START.md** - Get started in 5 minutes
- **docs/ARCHITECTURE.md** - Detailed system architecture
- **docs/QUICK_REFERENCE.md** - Command reference

## References

- Beej's Guide to Network Programming: https://beej.us/guide/bgnet/
- POSIX Threads Programming: https://computing.llnl.gov/tutorials/pthreads/
- TCP/IP Socket Programming: Stevens, W. Richard. "Unix Network Programming"

## Authors

Sahil Narkhede
Anshit Agarwal 

## License

Educational project for Computer Networks course (CSL3080).
