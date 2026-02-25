# Makefile for BitGossip P2P Network

CC = gcc
CFLAGS = -Wall -pthread -g -O2
LDFLAGS = -pthread -lm

SRC_DIR = src
SCRIPT_DIR = scripts

# Targets
all: seed peer

seed: $(SRC_DIR)/seed.c
	$(CC) $(CFLAGS) -o seed $(SRC_DIR)/seed.c $(LDFLAGS)

peer: $(SRC_DIR)/peer.c
	$(CC) $(CFLAGS) -o peer $(SRC_DIR)/peer.c $(LDFLAGS)

clean:
	rm -f seed peer
	rm -f output/*.txt

clean_output:
	rm -f output/*.txt

run_seeds: seed
	@echo "Starting seed nodes..."
	@./seed 5001 &
	@sleep 0.5
	@./seed 5002 &
	@sleep 0.5
	@./seed 5003 &
	@echo "Seed nodes started"

run_peers: peer
	@echo "Starting peer nodes..."
	@./peer 6001 &
	@sleep 1
	@./peer 6002 &
	@sleep 1
	@./peer 6003 &
	@sleep 1
	@./peer 6004 &
	@sleep 1
	@./peer 6005 &
	@echo "Peer nodes started"

stop:
	@echo "Stopping all nodes..."
	@pkill -9 seed 2>/dev/null || true
	@pkill -9 peer 2>/dev/null || true
	@echo "All nodes stopped"

test: all
	@echo "Running network test..."
	@bash $(SCRIPT_DIR)/test_network.sh

.PHONY: all clean clean_output run_seeds run_peers stop test
