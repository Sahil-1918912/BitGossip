# BitGossip - C Implementation Quick Start Guide

## ✅ What's Been Implemented

You now have a complete **C implementation** of the BitGossip P2P network with:

### Core Programs (C)
- **seed.c** → Compiled to `./seed` executable
- **peer.c** → Compiled to `./peer` executable

### Features Implemented
✓ Consensus-based peer registration (seed-level voting)
✓ Consensus-based peer removal (two-level: peer + seed)
✓ Gossip protocol with hash-based deduplication
✓ Power-law topology for neighbor selection
✓ System ping-based liveness detection
✓ Multithreaded architecture (pthreads)
✓ Thread-safe data structures with mutexes
✓ TCP socket communication
✓ Timestamped logging to files

## 📁 Project Structure

```
BitGossip/
├── src/
│   ├── seed.c           ✓ C source for seed nodes
│   └── peer.c           ✓ C source for peer nodes
├── scripts/
│   ├── start_seeds.sh   ✓ Start all seeds
│   ├── start_peers.sh   ✓ Start all peers
│   ├── stop_all.sh      ✓ Stop everything
│   ├── clean_output.sh  ✓ Clean logs
│   ├── test_network.sh  ✓ Automated test
│   └── monitor_logs.sh  ✓ Real-time monitoring
├── docs/
│   ├── ARCHITECTURE.md  ✓ System design
│   ├── QUICK_START.md   ✓ This file
│   └── QUICK_REFERENCE.md ✓ Command reference
├── output/              ✓ Log directory
├── seed                 ✓ Compiled seed executable (81 KB)
├── peer                 ✓ Compiled peer executable (54 KB)
├── Makefile             ✓ Build configuration
├── config.txt           ✓ Seed configuration
└── README.md            ✓ Complete documentation
```

## 🚀 Running the Network (3 Easy Steps)

### Step 1: Compile (Already Done!)
```bash
# Executables are already compiled:
ls -lh seed peer
```

### Step 2: Start Seeds
```bash
scripts/start_seeds.sh
# Or use make:
make run_seeds
```

Wait 2 seconds, then:

### Step 3: Start Peers
```bash
scripts/start_peers.sh
# Or use make:
make run_peers
```

**That's it!** The network is now running.

## 📊 Monitoring

### Watch Logs in Real-Time
```bash
scripts/monitor_logs.sh
```

### Watch Specific Node
```bash
tail -f output/seed_5001.txt   # Seed node
tail -f output/peer_6001.txt   # Peer node
```

## 🛑 Stopping

```bash
scripts/stop_all.sh
# Or use make:
make stop
```

## 🧪 Testing

### Quick Test
```bash
scripts/test_network.sh
# Or use make:
make test
```

This will:
1. Compile programs
2. Start 3 seeds and 5 peers
3. Test consensus registration
4. Test gossip propagation
5. Kill a peer and test dead node detection
6. Show statistics

### Manual Test - Dead Node Detection
```bash
# After network is running
ps aux | grep "./peer 6003"   # Find PID
kill -9 <PID>                 # Kill it

# Watch other peers detect and report it
tail -f output/peer_6001.txt
```

## 📝 Important Files

### Deliverables (As Per Assignment)
- ✓ **src/seed.c** - Seed node source
- ✓ **src/peer.c** - Peer node source
- ✓ **config.txt** - Configuration file
- ✓ **output/seed_*.txt** - Seed logs
- ✓ **output/peer_*.txt** - Peer logs
- ✓ **Makefile** - Build configuration

### Configuration
**config.txt** contains seed addresses:
```
127.0.0.1,5001
127.0.0.1,5002
127.0.0.1,5003
```

## 🔧 Rebuilding

If you modify the C source:

```bash
make clean      # Remove old executables
make all        # Rebuild everything
```

Or:
```bash
./compile.sh
```

## 💡 What You'll See

### Seed Logs
```
[2026-02-25 18:30:01.234] Seed running at 127.0.0.1:5001 (Quorum: 2/3)
[2026-02-25 18:30:05.567] Registration request from 127.0.0.1:6001
[2026-02-25 18:30:05.678] Proposing registration for peer 127.0.0.1:6001
[2026-02-25 18:30:06.123] CONSENSUS REACHED: Peer 127.0.0.1:6001 registered (2/3 votes)
```

### Peer Logs
```
[2026-02-25 18:30:05.234] Peer starting at 127.0.0.1:6001
[2026-02-25 18:30:05.345] Registered with seed 127.0.0.1:5001
[2026-02-25 18:30:05.456] Successfully registered with 2 seeds
[2026-02-25 18:30:05.567] Selected 3 neighbors using power-law distribution
[2026-02-25 18:30:05.678] Connected to peer 127.0.0.1:6002
[2026-02-25 18:30:10.123] GOSSIP GENERATED: 2026-02-25 18:30:10.123:127.0.0.1:0
[2026-02-25 18:30:11.234] GOSSIP RECEIVED: ... (from 127.0.0.1:6002)
```

## ⚠️ Troubleshooting

### "Address already in use"
```bash
scripts/stop_all.sh
# OR
pkill -9 seed
pkill -9 peer
```

### Compilation fails
```bash
# Install build tools (Ubuntu/Debian)
sudo apt-get install build-essential

# Rebuild
make clean && make all
```

### No output in logs
- Make sure `output/` directory exists
- Check file permissions: `ls -la output/`
- Seeds must be running before peers

## 📚 Documentation

- **README.md** - Complete implementation guide
- **docs/ARCHITECTURE.md** - System architecture and diagrams
- **docs/QUICK_REFERENCE.md** - Command reference
- **docs/QUICK_START.md** - This file

## 🎯 Key Differences from Python Version

| Aspect | Python Version | C Version |
|--------|---------------|-----------|
| Compile | No | Yes (`make all`) |
| Run | `python3 seed.py <port>` | `./seed <port>` |
| Threading | `threading` module | POSIX pthreads |
| Networking | Python `socket` | BSD sockets |
| Speed | Slower | Faster |
| Memory | Higher | Lower (81KB + 54KB) |

## ✨ Next Steps

1. ✅ Programs are compiled and ready
2. ✅ Run `./start_seeds.sh` then `./start_peers.sh`
3. ✅ Watch logs with `./monitor_logs.sh`
4. ✅ Test with `./test_network.sh`
5. ✅ Submit: seed.c, peer.c, config.txt, output files

## 🎓 Assignment Checklist

- [x] Implemented in C
- [x] Seed nodes with consensus (seed.c)
- [x] Peer nodes with gossip (peer.c)
- [x] Configuration file (config.txt)
- [x] Output files (output/seed_*.txt, output/peer_*.txt)
- [x] Consensus-based registration
- [x] Consensus-based removal
- [x] Power-law topology
- [x] Gossip protocol
- [x] Liveness detection (ping)
- [x] Two-level consensus
- [x] Socket programming (TCP)
- [x] Multithreading (pthreads)
- [x] Makefile for compilation

## 📞 Need Help?

Check the comprehensive documentation in **README_C.md** for:
- Detailed protocol specifications
- Message formats
- Consensus mechanisms
- Security considerations
- Performance tuning
- Advanced testing scenarios

---

**All ready to go! 🚀**

Run `scripts/start_seeds.sh` to begin!
