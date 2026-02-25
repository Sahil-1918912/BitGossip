# seed.py

import socket
import threading
import sys
import json
import time

peer_list = set()
registration_votes = {}
removal_votes = {}

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
    print(msg)
    with open(f"output/seed_{SEED_PORT}.txt", "a") as f:
        f.write(msg + "\n")

def handle_client(conn):
    global peer_list
    data = conn.recv(4096).decode()
    if data.startswith("REGISTER"):
        _, ip, port = data.split(":")
        peer_list.add((ip, int(port)))
        log(f"Registered peer {ip}:{port}")
        conn.send("REGISTERED".encode())
    conn.close()

def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((SEED_IP, SEED_PORT))
    server.listen(10)
    log(f"Seed running at {SEED_IP}:{SEED_PORT}")

    while True:
        conn, addr = server.accept()
        threading.Thread(target=handle_client, args=(conn,)).start()

if __name__ == "__main__":
    load_seeds()
    start_server()