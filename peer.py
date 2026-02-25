# peer.py

import socket
import threading
import sys
import json
import time
import hashlib
import random
from datetime import datetime
import subprocess

class Peer:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = int(port)
        self.seed_list = []
        self.quorum = None
        self.registered_seeds = []
        
        # Peer network
        self.neighbors = set()  # Set of (ip, port) tuples
        self.neighbor_connections = {}  # {(ip, port): socket}
        self.neighbor_lock = threading.Lock()
        
        # Gossip protocol
        self.message_list = {}  # {hash: (timestamp, sender_ip, msg)}
        self.ml_lock = threading.Lock()
        self.message_count = 0
        self.max_messages = 10
        
        # Liveness detection
        self.suspicion_count = {}  # {peer_id: count}
        self.dead_reports = {}  # {peer_id: set of reporter peers}
        self.liveness_lock = threading.Lock()
        self.reported_dead = set()  # Peers already reported as dead to seeds
        
        # Server
        self.server_socket = None
        self.running = True
        
    def load_seeds(self):
        """Load seed node information from config file"""
        with open("config.txt", "r") as f:
            for line in f:
                ip, port = line.strip().split(",")
                self.seed_list.append((ip, int(port)))
        self.quorum = len(self.seed_list) // 2 + 1
        self.log(f"Loaded {len(self.seed_list)} seeds, quorum: {self.quorum}")
    
    def log(self, msg):
        """Log message to console and file"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        log_msg = f"[{timestamp}] {msg}"
        print(log_msg)
        with open(f"output/peer_{self.port}.txt", "a") as f:
            f.write(log_msg + "\n")
    
    def send_to_seed(self, ip, port, message):
        """Send a message to a seed node"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((ip, port))
            sock.send(message.encode())
            response = sock.recv(4096).decode()
            sock.close()
            return response
        except Exception as e:
            self.log(f"Error contacting seed {ip}:{port} - {e}")
            return None
    
    def register_with_seeds(self):
        """Register with at least quorum seeds"""
        random.shuffle(self.seed_list)
        
        for seed_ip, seed_port in self.seed_list:
            msg = f"REGISTER:{self.ip}:{self.port}"
            response = self.send_to_seed(seed_ip, seed_port, msg)
            
            if response and "REGISTERED" in response:
                self.registered_seeds.append((seed_ip, seed_port))
                self.log(f"Registered with seed {seed_ip}:{seed_port}")
                
                if len(self.registered_seeds) >= self.quorum:
                    self.log(f"Successfully registered with {len(self.registered_seeds)} seeds")
                    return True
        
        if len(self.registered_seeds) >= self.quorum:
            return True
        else:
            self.log(f"ERROR: Could only register with {len(self.registered_seeds)} seeds, needed {self.quorum}")
            return False
    
    def get_peer_lists(self):
        """Get peer lists from registered seeds"""
        all_peers = set()
        
        for seed_ip, seed_port in self.registered_seeds:
            response = self.send_to_seed(seed_ip, seed_port, "GET_PEERS")
            if response:
                try:
                    peers = json.loads(response)
                    peer_tuples = [tuple(p) for p in peers]
                    all_peers.update(peer_tuples)
                    self.log(f"Received {len(peer_tuples)} peers from seed {seed_ip}:{seed_port}")
                except Exception as e:
                    self.log(f"Error parsing peer list from {seed_ip}:{seed_port}: {e}")
        
        # Remove self from peer list
        all_peers.discard((self.ip, self.port))
        
        self.log(f"Total unique peers discovered: {len(all_peers)}")
        return list(all_peers)
    
    def select_neighbors_powerlaw(self, peer_list):
        """Select neighbors following power-law distribution"""
        if not peer_list:
            self.log("No peers available for connection")
            return []
        
        n = len(peer_list)
        
        # Power-law: P(k) ~ k^(-gamma), typically gamma between 2 and 3
        # We'll use a simplified approach: assign weights inversely proportional to rank
        # More connections to fewer nodes, creating hubs
        
        if n <= 3:
            # If few peers, connect to all
            return peer_list
        
        # Determine number of neighbors: between 2 and min(n, 5)
        # Following power-law, most nodes have low degree, few have high degree
        # Random choice biased towards lower degrees
        degree_options = list(range(2, min(n, 6)))
        weights = [1.0 / (k ** 1.5) for k in degree_options]
        total_weight = sum(weights)
        weights = [w / total_weight for w in weights]
        
        num_neighbors = random.choices(degree_options, weights=weights)[0]
        
        # Select neighbors randomly (could enhance with preferential attachment)
        selected = random.sample(peer_list, num_neighbors)
        
        self.log(f"Selected {len(selected)} neighbors using power-law distribution")
        return selected
    
    def connect_to_peer(self, peer_ip, peer_port):
        """Establish connection to a peer"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((peer_ip, peer_port))
            
            # Send introduction
            intro = f"HELLO:{self.ip}:{self.port}"
            sock.send(intro.encode())
            
            with self.neighbor_lock:
                self.neighbors.add((peer_ip, peer_port))
                self.neighbor_connections[(peer_ip, peer_port)] = sock
            
            self.log(f"Connected to peer {peer_ip}:{peer_port}")
            
            # Start listening for messages from this peer
            threading.Thread(target=self.listen_to_peer, args=(sock, peer_ip, peer_port), daemon=True).start()
            
            return True
        except Exception as e:
            self.log(f"Failed to connect to {peer_ip}:{peer_port}: {e}")
            return False
    
    def listen_to_peer(self, sock, peer_ip, peer_port):
        """Listen for messages from a connected peer"""
        peer_id = f"{peer_ip}:{peer_port}"
        
        try:
            while self.running:
                data = sock.recv(4096).decode()
                if not data:
                    break
                
                # Handle different message types
                if data.startswith("GOSSIP:"):
                    self.handle_gossip_message(data, peer_ip)
                elif data.startswith("SUSPECT:"):
                    self.handle_suspect_message(data, peer_ip)
                elif data.startswith("PING"):
                    sock.send("PONG".encode())
        except Exception as e:
            self.log(f"Connection lost with {peer_id}: {e}")
        finally:
            with self.neighbor_lock:
                self.neighbors.discard((peer_ip, peer_port))
                if (peer_ip, peer_port) in self.neighbor_connections:
                    del self.neighbor_connections[(peer_ip, peer_port)]
    
    def handle_gossip_message(self, data, sender_ip):
        """Handle incoming gossip message"""
        # Format: GOSSIP:<timestamp>:<ip>:<msg#>
        try:
            parts = data.split(":", 1)
            message = parts[1]
            
            msg_hash = hashlib.sha256(message.encode()).hexdigest()
            
            with self.ml_lock:
                if msg_hash not in self.message_list:
                    # First time seeing this message
                    self.message_list[msg_hash] = (message, sender_ip)
                    self.log(f"GOSSIP RECEIVED: {message} (from {sender_ip})")
                    
                    # Forward to all neighbors except sender
                    self.forward_gossip(data, sender_ip)
                # Else: duplicate, ignore
        except Exception as e:
            self.log(f"Error handling gossip: {e}")
    
    def forward_gossip(self, gossip_msg, exclude_ip):
        """Forward gossip message to all neighbors except sender"""
        with self.neighbor_lock:
            for (peer_ip, peer_port), sock in self.neighbor_connections.items():
                if peer_ip != exclude_ip:
                    try:
                        sock.send(gossip_msg.encode())
                    except Exception as e:
                        self.log(f"Error forwarding to {peer_ip}:{peer_port}: {e}")
    
    def handle_suspect_message(self, data, sender_ip):
        """Handle suspicion report from another peer"""
        # Format: SUSPECT:<dead_ip>:<dead_port>:<sender_ip>
        try:
            _, dead_ip, dead_port, reporter_ip = data.split(":")
            peer_id = f"{dead_ip}:{dead_port}"
            
            with self.liveness_lock:
                if peer_id not in self.dead_reports:
                    self.dead_reports[peer_id] = set()
                self.dead_reports[peer_id].add(reporter_ip)
                
                # Check if we have consensus among neighbors
                report_count = len(self.dead_reports[peer_id])
                total_neighbors = len(self.neighbors)
                
                if total_neighbors > 0:
                    neighbor_quorum = (total_neighbors // 2) + 1
                    
                    if report_count >= neighbor_quorum and peer_id not in self.reported_dead:
                        self.log(f"PEER CONSENSUS: {peer_id} confirmed dead by {report_count} peers")
                        self.report_dead_to_seeds(dead_ip, dead_port)
                        self.reported_dead.add(peer_id)
        except Exception as e:
            self.log(f"Error handling suspect message: {e}")
    
    def generate_gossip_messages(self):
        """Generate gossip messages every 5 seconds"""
        time.sleep(2)  # Wait a bit for network to stabilize
        
        while self.message_count < self.max_messages and self.running:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            message = f"{timestamp}:{self.ip}:{self.message_count}"
            gossip_msg = f"GOSSIP:{message}"
            
            self.message_count += 1
            
            # Add to own message list
            msg_hash = hashlib.sha256(message.encode()).hexdigest()
            with self.ml_lock:
                self.message_list[msg_hash] = (message, self.ip)
            
            self.log(f"GOSSIP GENERATED: {message}")
            
            # Send to all neighbors
            with self.neighbor_lock:
                for (peer_ip, peer_port), sock in list(self.neighbor_connections.items()):
                    try:
                        sock.send(gossip_msg.encode())
                    except Exception as e:
                        self.log(f"Error sending gossip to {peer_ip}:{peer_port}: {e}")
            
            time.sleep(5)
        
        self.log(f"Finished generating {self.message_count} gossip messages")
    
    def check_liveness(self):
        """Periodically check liveness of neighbors"""
        time.sleep(5)  # Wait for connections to establish
        
        while self.running:
            time.sleep(3)  # Check every 3 seconds
            
            with self.neighbor_lock:
                neighbors_to_check = list(self.neighbor_connections.items())
            
            for (peer_ip, peer_port), sock in neighbors_to_check:
                peer_id = f"{peer_ip}:{peer_port}"
                
                # Use system ping for liveness check
                is_alive = self.ping_peer(peer_ip)
                
                if not is_alive:
                    with self.liveness_lock:
                        if peer_id not in self.suspicion_count:
                            self.suspicion_count[peer_id] = 0
                        self.suspicion_count[peer_id] += 1
                        
                        # After 3 failed pings, suspect the peer
                        if self.suspicion_count[peer_id] >= 3:
                            self.log(f"SUSPECTING peer {peer_id} (failed {self.suspicion_count[peer_id]} pings)")
                            self.broadcast_suspicion(peer_ip, peer_port)
                else:
                    # Reset suspicion if peer responds
                    with self.liveness_lock:
                        if peer_id in self.suspicion_count:
                            self.suspicion_count[peer_id] = 0
    
    def ping_peer(self, peer_ip):
        """Ping a peer using system ping"""
        try:
            # Use system ping (1 packet, 1 second timeout)
            result = subprocess.run(
                ['ping', '-c', '1', '-W', '1', peer_ip],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            return result.returncode == 0
        except Exception:
            return False
    
    def broadcast_suspicion(self, dead_ip, dead_port):
        """Broadcast suspicion to all neighbors"""
        peer_id = f"{dead_ip}:{dead_port}"
        suspect_msg = f"SUSPECT:{dead_ip}:{dead_port}:{self.ip}"
        
        with self.neighbor_lock:
            for (peer_ip, peer_port), sock in list(self.neighbor_connections.items()):
                if peer_ip != dead_ip:  # Don't send to suspected peer
                    try:
                        sock.send(suspect_msg.encode())
                    except Exception as e:
                        self.log(f"Error broadcasting suspicion to {peer_ip}:{peer_port}: {e}")
        
        # Also add own suspicion to dead_reports for consensus
        with self.liveness_lock:
            if peer_id not in self.dead_reports:
                self.dead_reports[peer_id] = set()
            self.dead_reports[peer_id].add(self.ip)
            
            # Check consensus
            report_count = len(self.dead_reports[peer_id])
            total_neighbors = len(self.neighbors)
            
            if total_neighbors > 0:
                neighbor_quorum = (total_neighbors // 2) + 1
                
                if report_count >= neighbor_quorum and peer_id not in self.reported_dead:
                    self.log(f"PEER CONSENSUS: {peer_id} confirmed dead by {report_count} peers")
                    self.report_dead_to_seeds(dead_ip, dead_port)
                    self.reported_dead.add(peer_id)
    
    def report_dead_to_seeds(self, dead_ip, dead_port):
        """Report dead node to all registered seeds"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        msg = f"Dead Node:{dead_ip}:{dead_port}:{timestamp}:{self.ip}"
        
        self.log(f"REPORTING to seeds: Dead node {dead_ip}:{dead_port}")
        
        for seed_ip, seed_port in self.registered_seeds:
            try:
                response = self.send_to_seed(seed_ip, seed_port, msg)
                if response:
                    self.log(f"Dead node report acknowledged by seed {seed_ip}:{seed_port}")
            except Exception as e:
                self.log(f"Error reporting to seed {seed_ip}:{seed_port}: {e}")
    
    def handle_incoming_connection(self, conn, addr):
        """Handle incoming connection from another peer"""
        try:
            data = conn.recv(4096).decode()
            
            if data.startswith("HELLO:"):
                # Format: HELLO:ip:port
                _, peer_ip, peer_port = data.split(":")
                peer_port = int(peer_port)
                
                self.log(f"Incoming connection from {peer_ip}:{peer_port}")
                
                with self.neighbor_lock:
                    self.neighbors.add((peer_ip, peer_port))
                    self.neighbor_connections[(peer_ip, peer_port)] = conn
                
                # Listen for messages from this peer
                threading.Thread(target=self.listen_to_peer, args=(conn, peer_ip, peer_port), daemon=True).start()
        except Exception as e:
            self.log(f"Error handling incoming connection: {e}")
            conn.close()
    
    def start_server(self):
        """Start peer server to accept incoming connections"""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.ip, self.port))
        self.server_socket.listen(10)
        self.log(f"Peer server started at {self.ip}:{self.port}")
        
        while self.running:
            try:
                conn, addr = self.server_socket.accept()
                threading.Thread(target=self.handle_incoming_connection, args=(conn, addr), daemon=True).start()
            except Exception as e:
                if self.running:
                    self.log(f"Server error: {e}")
    
    def run(self):
        """Main peer execution"""
        self.log(f"Peer starting at {self.ip}:{self.port}")
        
        # Load seeds
        self.load_seeds()
        
        # Start server in background
        threading.Thread(target=self.start_server, daemon=True).start()
        
        # Register with seeds
        if not self.register_with_seeds():
            self.log("Failed to register with sufficient seeds. Exiting.")
            return
        
        # Get peer lists
        peer_list = self.get_peer_lists()
        
        # Select neighbors using power-law distribution
        selected_peers = self.select_neighbors_powerlaw(peer_list)
        
        # Connect to selected neighbors
        for peer_ip, peer_port in selected_peers:
            self.connect_to_peer(peer_ip, peer_port)
        
        time.sleep(1)
        self.log(f"Connected to {len(self.neighbors)} neighbors")
        
        # Start gossip message generation
        threading.Thread(target=self.generate_gossip_messages, daemon=True).start()
        
        # Start liveness checking
        threading.Thread(target=self.check_liveness, daemon=True).start()
        
        # Keep running
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            self.log("Shutting down...")
            self.running = False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python peer.py <port>")
        sys.exit(1)
    
    peer = Peer("127.0.0.1", sys.argv[1])
    peer.run()
