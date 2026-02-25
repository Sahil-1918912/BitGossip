#!/usr/bin/env python3
"""
Monitor network statistics from log files
"""

import os
import re
from collections import defaultdict

def analyze_logs():
    output_dir = "output"
    
    if not os.path.exists(output_dir):
        print("No output directory found. Run the network first.")
        return
    
    print("=" * 60)
    print("BitGossip Network Statistics")
    print("=" * 60)
    print()
    
    # Analyze seed logs
    print("SEED NODES:")
    print("-" * 60)
    
    seed_files = [f for f in os.listdir(output_dir) if f.startswith("seed_")]
    
    for seed_file in sorted(seed_files):
        port = seed_file.split("_")[1].split(".")[0]
        filepath = os.path.join(output_dir, seed_file)
        
        with open(filepath, 'r') as f:
            content = f.read()
        
        registrations = len(re.findall(r"CONSENSUS REACHED.*registered", content))
        removals = len(re.findall(r"CONSENSUS REACHED.*removed", content))
        proposals = len(re.findall(r"Proposing registration", content))
        dead_reports = len(re.findall(r"Dead node report", content))
        
        print(f"Seed {port}:")
        print(f"  Registration proposals: {proposals}")
        print(f"  Successful registrations: {registrations}")
        print(f"  Dead node reports: {dead_reports}")
        print(f"  Successful removals: {removals}")
        print()
    
    # Analyze peer logs
    print("\nPEER NODES:")
    print("-" * 60)
    
    peer_files = [f for f in os.listdir(output_dir) if f.startswith("peer_")]
    
    total_gossip_generated = 0
    total_gossip_received = 0
    
    for peer_file in sorted(peer_files):
        port = peer_file.split("_")[1].split(".")[0]
        filepath = os.path.join(output_dir, peer_file)
        
        with open(filepath, 'r') as f:
            content = f.read()
        
        neighbors = len(re.findall(r"Connected to peer", content))
        incoming = len(re.findall(r"Incoming connection from", content))
        generated = len(re.findall(r"GOSSIP GENERATED", content))
        received = len(re.findall(r"GOSSIP RECEIVED", content))
        suspicions = len(re.findall(r"SUSPECTING peer", content))
        reports = len(re.findall(r"REPORTING to seeds", content))
        
        total_gossip_generated += generated
        total_gossip_received += received
        
        print(f"Peer {port}:")
        print(f"  Outgoing connections: {neighbors}")
        print(f"  Incoming connections: {incoming}")
        print(f"  Total degree: {neighbors + incoming}")
        print(f"  Gossip generated: {generated}")
        print(f"  Gossip received: {received}")
        print(f"  Suspicions: {suspicions}")
        print(f"  Dead node reports: {reports}")
        print()
    
    # Summary
    print("\nNETWORK SUMMARY:")
    print("-" * 60)
    print(f"Total seeds: {len(seed_files)}")
    print(f"Total peers: {len(peer_files)}")
    print(f"Total gossip messages generated: {total_gossip_generated}")
    print(f"Total gossip messages received: {total_gossip_received}")
    
    if total_gossip_generated > 0:
        propagation_rate = (total_gossip_received / total_gossip_generated) * 100
        print(f"Message propagation rate: {propagation_rate:.1f}%")
    
    print()
    print("=" * 60)

if __name__ == "__main__":
    analyze_logs()
