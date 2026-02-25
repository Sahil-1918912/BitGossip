# BitGossip Network Architecture

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    SEED NODE LAYER                          │
│  (Consensus-based Membership Management)                    │
│                                                             │
│   ┌──────────┐      ┌──────────┐      ┌──────────┐          │
│   │ Seed     │◄────►│ Seed     │◄────►│ Seed     │          │
│   │ :5001    │      │ :5002    │      │ :5003    │          │
│   └────┬─────┘      └────┬─────┘      └────┬─────┘          │ 
│        │ Vote Exchange   │                  │               │
│        │ Proposals       │                  │               │
└────────┼─────────────────┼──────────────────┼───────────────┘
         │                 │                  │
         │  Registration   │   Peer Lists     │
         │  Dead Reports   │                  │
         │                 │                  │
┌────────▼─────────────────▼──────────────────▼──────────────┐
│                    PEER NODE LAYER                         │
│  (Gossip Propagation & Liveness Detection)                 │
│                                                            │
│     ┌────────┐         ┌────────┐         ┌────────┐       │
│     │ Peer   │◄───────►│ Peer   │◄───────►│ Peer   │       │
│     │ :6001  │         │ :6002  │         │ :6003  │       │
│     └───┬────┘         └───┬────┘         └───┬────┘       │
│         │                  │  Gossip                       │
│         │  Power-Law       │  Messages                     │ 
│         │  Topology        │  Liveness                     │
│         │                  │  Checks                       │
│     ┌───▼────┐         ┌───▼────┐                          │
│     │ Peer   │◄───────►│ Peer   │                          │
│     │ :6004  │         │ :6005  │                          │
│     └────────┘         └────────┘                          │
└────────────────────────────────────────────────────────────┘
```

## Message Flow

### Registration Flow
```
Peer → Seed₁: REGISTER
      ↓
Seed₁ → Seed₂,₃: VOTE_REGISTER (proposal)
      ↓
Seed₂,₃ → Seed₁: VOTE_ACK
      ↓
Seed₁: Check quorum → Add to PL
      ↓
Seed₁ → Peer: REGISTERED
```

### Gossip Flow
```
Peer₁: Generate message M
      ↓
Peer₁ → All neighbors: GOSSIP:M
      ↓
Peer₂: Receive M (first time)
      ↓
Peer₂: Add hash(M) to ML
      ↓
Peer₂ → All neighbors except Peer₁: GOSSIP:M
      ↓
Peer₃: Receive M (first time) → Forward...
```

### Dead Node Detection Flow
```
Peer₁: Ping Peer₃ → No response (×3)
      ↓
Peer₁ → Neighbors: SUSPECT:Peer₃
      ↓
Peer₂, Peer₄: Verify Peer₃ unresponsive
      ↓
Peer₂, Peer₄ → Peer₁: Confirm suspicion
      ↓
Check peer quorum reached?
      ↓ Yes
Peer₁ → All Seeds: Dead Node:Peer₃
      ↓
Seed₁ → Seed₂,₃: VOTE_REMOVE
      ↓
Check seed quorum reached?
      ↓ Yes
Seeds: Remove Peer₃ from PL
```

## Component Responsibilities

### Seed Nodes
```
┌─────────────────────────────────┐
│ Seed Node                       │
├─────────────────────────────────┤
│ • Load config.txt               │
│ • Accept peer registrations     │
│ • Propose to other seeds        │
│ • Vote on proposals             │
│ • Maintain Peer List (PL)       │
│ • Provide peer lists to joiners │
│ • Process dead node reports     │
│ • Coordinate removal consensus  │
└─────────────────────────────────┘
```

### Peer Nodes
```
┌─────────────────────────────────┐
│ Peer Node                       │
├─────────────────────────────────┤
│ • Load seed list                │
│ • Register with quorum seeds    │
│ • Get peer lists from seeds     │
│ • Select neighbors (power-law)  │
│ • Connect to selected peers     │
│ • Accept incoming connections   │
│ • Generate gossip messages      │
│ • Forward received gossip       │
│ • Maintain Message List (ML)    │
│ • Ping neighbors periodically   │
│ • Detect unresponsive peers     │
│ • Coordinate peer consensus     │
│ • Report dead nodes to seeds    │
└─────────────────────────────────┘
```

## Data Structures

### Seed Node
```python
peer_list: Set[(ip, port)]              # Registered peers
registration_votes: {peer_id: {seed_port: vote}}
removal_votes: {peer_id: {seed_port: vote}}
SEED_LIST: [(ip, port)]                 # All seeds
QUORUM: int                             # ⌊n/2⌋ + 1
```

### Peer Node
```python
neighbors: Set[(ip, port)]              # Connected peers
neighbor_connections: {(ip, port): socket}
message_list: {hash: (msg, sender)}     # Deduplication
suspicion_count: {peer_id: count}       # Failed pings
dead_reports: {peer_id: Set[reporters]} # Peer consensus
reported_dead: Set[peer_id]             # Already reported
registered_seeds: [(ip, port)]          # Connected seeds
```

## Protocol States

### Peer Lifecycle
```
  [INIT]
    ↓
  Load seeds
    ↓
  [REGISTERING] ──(quorum reached)──→ [REGISTERED]
    │                                      ↓
    └────────(failed)──────→ [EXIT]   Get peer lists
                                           ↓
                                    Select neighbors
                                           ↓
                                   [CONNECTING] ──→ [CONNECTED]
                                                        ↓
                      ┌────────────────────────────────┴─────────┐
                      ↓                                          ↓
                  [GOSSIPING]                            [MONITORING]
                (Generate & forward)                  (Ping neighbors)
                      ↓                                          ↓
                      └──────────(running)────────────←──────────┘
```

### Node Health States
```
[HEALTHY] ──(1 failed ping)──→ [SUSPECTED] ──(reset)──→ [HEALTHY]
                                    │
                            (3 failed pings)
                                    ↓
                            [PEER_CONSENSUS]
                                    │
                            (quorum reached)
                                    ↓
                            [REPORTED_TO_SEEDS]
                                    │
                            (seed consensus)
                                    ↓
                                [REMOVED]
```

## Consensus Mechanisms

### Registration Consensus (Seed Level)
```
Required: ⌊n/2⌋ + 1 seeds vote YES
Process:
  1. First seed receives REGISTER
  2. Proposes to all other seeds
  3. Each seed votes independently
  4. Votes collected and counted
  5. If quorum → Add to PL
```

### Removal Consensus (Two-Level)
```
Peer Level:
  Required: ⌊neighbors/2⌋ + 1 confirm dead
  Process:
    1. Detecting peer broadcasts SUSPECT
    2. Neighbors verify independently
    3. Confirmations collected
    4. If quorum → Report to seeds

Seed Level:
  Required: ⌊n/2⌋ + 1 seeds vote YES
  Process:
    1. First seed receives Dead Node report
    2. Proposes removal to other seeds
    3. Each seed votes independently
    4. Votes collected and counted
    5. If quorum → Remove from PL
```

## Network Topology

### Power-Law Degree Distribution
```
Number of Peers
    ▲
    │ █
    │ █
    │ █
    │ █ █
    │ █ █
    │ █ █ █
    │ █ █ █ █
    │ █ █ █ █ █
    └───────────────► Degree (# of connections)
      2 3 4 5 6 7

Most peers: 2-3 connections (low degree)
Few peers: 5-7 connections (hubs, high degree)
```

## Security Model

### Attack Vectors & Mitigations
```
┌──────────────────────┬───────────────────────────────┐
│ Attack               │ Mitigation                    │
├──────────────────────┼───────────────────────────────┤
│ Sybil (fake peers)   │ Seed consensus required       │
│ False dead reports   │ Two-level consensus           │
│ Malicious seed       │ Quorum prevents single seed   │
│ Network partition    │ Power-law hubs maintain links │
│ Eclipse              │ Random neighbor selection     │
│ Message flooding     │ Deduplication via ML          │
└──────────────────────┴───────────────────────────────┘
```

### Quorum Requirements
```
For n = 3 seeds: Quorum = 2 (can survive 1 failure)
For n = 5 seeds: Quorum = 3 (can survive 2 failures)
For n = 7 seeds: Quorum = 4 (can survive 3 failures)

General: Quorum = ⌊n/2⌋ + 1 (Byzantine fault tolerance)
```

## Performance Characteristics

### Time Complexity
- Registration: O(n) where n = number of seeds
- Gossip propagation: O(log N) for N peers (typical small-world)
- Dead node detection: O(m) where m = number of neighbors
- Consensus voting: O(n) for n seeds

### Message Complexity
- Per registration: O(n²) seed-to-seed votes
- Per gossip: O(d) where d = average degree
- Per dead node: O(m + n) for m neighbors + n seeds

### Expected Delays
- Registration: ~1-2 seconds
- Gossip propagation: ~0.5-2 seconds network-wide
- Dead detection: ~10-15 seconds (3 pings × 3s interval + consensus)
