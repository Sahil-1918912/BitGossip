# BitGossip - Repository Organization

## 📂 Directory Structure

```
BitGossip/
├── src/                    Source Code
├── scripts/                Automation Scripts
├── docs/                   Documentation
├── output/                 Runtime Logs
├── config.txt              Network Configuration
├── Makefile                Build System
└── README.md               Main Documentation
```

## 📄 File Inventory

### Source Code (`src/`)
- **seed.c** (16 KB) - Seed node implementation with consensus voting
- **peer.c** (28 KB) - Peer node with gossip protocol and liveness detection

### Scripts (`scripts/`)
- **start_seeds.sh** - Launch all seed nodes
- **start_peers.sh** - Launch all peer nodes  
- **stop_all.sh** - Terminate all running nodes
- **clean_output.sh** - Remove log files
- **test_network.sh** - Automated comprehensive test
- **monitor_logs.sh** - Real-time colored log viewer
- **compile.sh** - Quick compilation helper

### Documentation (`docs/`)
- **QUICK_START.md** - Get started in 5 minutes
- **ARCHITECTURE.md** - System design and protocol details
- **QUICK_REFERENCE.md** - Command reference guide

### Configuration
- **config.txt** - Seed node addresses (IP,Port format)
- **Makefile** - Build and automation commands

### Generated Files
- **seed** (executable) - Compiled seed program
- **peer** (executable) - Compiled peer program
- **output/*.txt** - Runtime logs

## 🚀 Quick Navigation

### Getting Started
1. Read **README.md** - Complete implementation guide
2. Follow **docs/QUICK_START.md** - 3-step startup
3. Use **scripts/** - Pre-made automation

### Understanding the System
- **docs/ARCHITECTURE.md** - How it works
- **src/seed.c** - Consensus algorithm
- **src/peer.c** - Gossip protocol

### Running the Network
- **Compile:** `make all`
- **Start:** `scripts/start_seeds.sh` → `scripts/start_peers.sh`
- **Monitor:** `scripts/monitor_logs.sh`
- **Test:** `scripts/test_network.sh` or `make test`
- **Stop:** `scripts/stop_all.sh` or `make stop`

### Deliverables (Assignment Submission)
✅ src/seed.c  
✅ src/peer.c  
✅ config.txt  
✅ output/seed_*.txt (generated)  
✅ output/peer_*.txt (generated)  
✅ Makefile  
✅ README.md (documentation)

## 📋 Organization Benefits

### Clean Separation
- **Source** in `src/` - Easy to locate code
- **Scripts** in `scripts/` - Automation centralized
- **Docs** in `docs/` - References organized
- **Logs** in `output/` - Runtime data separated

### Easy Navigation
```
Need source code?        → src/
Need to run something?   → scripts/
Need documentation?      → docs/ or README.md
Need logs?              → output/
Need configuration?      → config.txt
```

### Professional Structure
- Standard layout (src/, docs/, scripts/)
- Clear naming conventions
- Logical grouping
- Easy to understand and maintain

## 🎯 Usage Patterns

### Daily Development
```bash
# Edit code
vim src/peer.c

# Rebuild
make all

# Test quickly
scripts/start_seeds.sh
scripts/start_peers.sh
```

### Testing & Debugging
```bash
# Run full test
make test

# Monitor logs
scripts/monitor_logs.sh

# Check specific node
tail -f output/peer_6001.txt
```

### Cleanup
```bash
# Stop everything
make stop

# Clean logs
make clean_output

# Full rebuild
make clean && make all
```

## 📖 Documentation Flow

1. **README.md** → Overview & Quick Start
2. **docs/QUICK_START.md** → Detailed startup guide
3. **docs/ARCHITECTURE.md** → Deep dive into design
4. **docs/QUICK_REFERENCE.md** → Command cheat sheet

## ✨ What Was Removed

- ❌ Python files (seed.py, peer.py, analyze_network.py)
- ❌ Python-specific documentation
- ❌ Redundant README files
- ❌ Scattered scripts

## ✅ What Was Organized

- ✅ All C source code → `src/`
- ✅ All shell scripts → `scripts/`
- ✅ All documentation → `docs/`
- ✅ Clean project root
- ✅ Professional structure

---

**Everything is now organized, clean, and easy to comprehend!**

Start with: **README.md** or **docs/QUICK_START.md**
