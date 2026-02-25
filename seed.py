# seed.py

import socket
import threading
import sys
import json
import time
from datetime import datetime

peer_list = set()
registration_votes = {}  # {peer_id: {seed_port: vote}}
removal_votes = {}  # {peer_id: {seed_port: vote}}
peer_list_lock = threading.Lock()
registration_lock = threading.Lock()
removal_lock = threading.Lock()

SEED_IP = "127.0.0.1"
SEED_PORT = int(sys.argv[1])
QUORUM = None
SEED_LIST = []

def load_seeds():
    global SEED_LIST, QUORUM
    with open("config.txt", "r") as f:
        for line in f:
            ip, port = line.strip().split(",")
            SEED_LIST.append((ip, int(port)))
    QUORUM = len(SEED_LIST)//2 + 1

def log(msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    log_msg = f"[{timestamp}] {msg}"
    print(log_msg)
    with open(f"output/seed_{SEED_PORT}.txt", "a") as f:
        f.write(log_msg + "\n")

def send_to_seed(ip, port, message):
    """Send a message to another seed node"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2)
        sock.connect((ip, port))
        sock.send(message.encode())
        response = sock.recv(4096).decode()
        sock.close()
        return response
    except Exception as e:
        return None

def propose_registration(peer_ip, peer_port):
    """Propose peer registration to other seeds"""
    peer_id = f"{peer_ip}:{peer_port}"
    log(f"Proposing registration for peer {peer_id}")
    
    with registration_lock:
        if peer_id not in registration_votes:
            registration_votes[peer_id] = {}
        registration_votes[peer_id][SEED_PORT] = True
    
    # Send proposal to other seeds
    for seed_ip, seed_port in SEED_LIST:
        if seed_port != SEED_PORT:
            msg = f"VOTE_REGISTER:{peer_ip}:{peer_port}:{SEED_PORT}"
            threading.Thread(target=send_to_seed, args=(seed_ip, seed_port, msg)).start()
    
    # Wait a bit for votes to come in
    time.sleep(1)
    
    # Check consensus
    with registration_lock:
        votes = registration_votes.get(peer_id, {})
        vote_count = sum(votes.values())
        
        if vote_count >= QUORUM:
            with peer_list_lock:
                peer_list.add((peer_ip, int(peer_port)))
            log(f"CONSENSUS REACHED: Peer {peer_id} registered ({vote_count}/{len(SEED_LIST)} votes)")
            return True
        else:
            log(f"CONSENSUS FAILED: Peer {peer_id} registration rejected ({vote_count}/{len(SEED_LIST)} votes)")
            return False

def propose_removal(dead_ip, dead_port, reporter_ip):
    """Propose peer removal to other seeds"""
    peer_id = f"{dead_ip}:{dead_port}"
    log(f"Proposing removal for peer {peer_id} (reported by {reporter_ip})")
    
    with removal_lock:
        if peer_id not in removal_votes:
            removal_votes[peer_id] = {}
        removal_votes[peer_id][SEED_PORT] = True
    
    # Send proposal to other seeds
    for seed_ip, seed_port in SEED_LIST:
        if seed_port != SEED_PORT:
            msg = f"VOTE_REMOVE:{dead_ip}:{dead_port}:{SEED_PORT}"
            threading.Thread(target=send_to_seed, args=(seed_ip, seed_port, msg)).start()
    
    # Wait a bit for votes to come in
    time.sleep(1)
    
    # Check consensus
    with removal_lock:
        votes = removal_votes.get(peer_id, {})
        vote_count = sum(votes.values())
        
        if vote_count >= QUORUM:
            with peer_list_lock:
                peer_list.discard((dead_ip, int(dead_port)))
            log(f"CONSENSUS REACHED: Peer {peer_id} removed ({vote_count}/{len(SEED_LIST)} votes)")
            return True
        else:
            log(f"CONSENSUS PENDING: Peer {peer_id} removal ({vote_count}/{len(SEED_LIST)} votes)")
            return False

def handle_client(conn, addr):
    """Handle incoming connections from peers and other seeds"""
    global peer_list
    try:
        data = conn.recv(4096).decode()
        
        if data.startswith("REGISTER"):
            # Format: REGISTER:ip:port
            _, ip, port = data.split(":")
            log(f"Registration request from {ip}:{port}")
            
            # Initiate consensus
            success = propose_registration(ip, port)
            
            if success:
                conn.send("REGISTERED".encode())
            else:
                conn.send("REGISTRATION_PENDING".encode())
        
        elif data.startswith("GET_PEERS"):
            # Format: GET_PEERS
            with peer_list_lock:
                peers_json = json.dumps(list(peer_list))
            log(f"Sending peer list to {addr[0]}:{addr[1]} - {len(peer_list)} peers")
            conn.send(peers_json.encode())
        
        elif data.startswith("VOTE_REGISTER"):
            # Format: VOTE_REGISTER:peer_ip:peer_port:proposer_seed_port
            _, peer_ip, peer_port, proposer_port = data.split(":")
            peer_id = f"{peer_ip}:{peer_port}"
            
            with registration_lock:
                if peer_id not in registration_votes:
                    registration_votes[peer_id] = {}
                registration_votes[peer_id][SEED_PORT] = True
            
            log(f"Voted YES for registration of {peer_id} (proposed by seed {proposer_port})")
            conn.send("VOTE_ACK".encode())
        
        elif data.startswith("VOTE_REMOVE"):
            # Format: VOTE_REMOVE:peer_ip:peer_port:proposer_seed_port
            _, peer_ip, peer_port, proposer_port = data.split(":")
            peer_id = f"{peer_ip}:{peer_port}"
            
            with removal_lock:
                if peer_id not in removal_votes:
                    removal_votes[peer_id] = {}
                removal_votes[peer_id][SEED_PORT] = True
            
            log(f"Voted YES for removal of {peer_id} (proposed by seed {proposer_port})")
            conn.send("VOTE_ACK".encode())
        
        elif data.startswith("Dead Node"):
            # Format: Dead Node:DeadNode.IP:DeadNode.Port:timestamp:reporter.IP
            parts = data.split(":")
            dead_ip = parts[1]
            dead_port = parts[2]
            reporter_ip = parts[4]
            
            log(f"Dead node report: {dead_ip}:{dead_port} from {reporter_ip}")
            
            # Initiate consensus for removal
            propose_removal(dead_ip, dead_port, reporter_ip)
            conn.send("REPORT_RECEIVED".encode())
    
    except Exception as e:
        log(f"Error handling client: {e}")
    finally:
        conn.close()

def start_server():
    """Start the seed server"""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((SEED_IP, SEED_PORT))
    server.listen(10)
    log(f"Seed running at {SEED_IP}:{SEED_PORT} (Quorum: {QUORUM}/{len(SEED_LIST)})")

    while True:
        conn, addr = server.accept()
        threading.Thread(target=handle_client, args=(conn, addr)).start()

if __name__ == "__main__":
    load_seeds()
    start_server()