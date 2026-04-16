// seed.c
// Seed node implementation with consensus-based membership management

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// Core limits and networking constants
#define MAX_PEERS 100
#define MAX_SEEDS 10
#define MAX_BUFFER 4096
#define SEED_IP "127.0.0.1"

// Basic node endpoint
typedef struct {
    char ip[16];
    int port;
} Node;

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

typedef struct {
    VoteRecord records[MAX_PEERS];
    int count;
    pthread_mutex_t lock;
} VoteList;

// Global shared state
PeerList peer_list = {.count = 0};
VoteList registration_votes = {.count = 0};
VoteList removal_votes = {.count = 0};
Node seed_list[MAX_SEEDS];
int seed_count = 0;
int quorum = 0;
int seed_port = 0;
FILE *log_file = NULL;

// Function declarations
void load_seeds();
void log_msg(const char *msg);
void *handle_client(void *arg);
void start_server();
int send_to_seed(const char *ip, int port, const char *message, char *response);
int propose_registration(const char *peer_ip, int peer_port);
int propose_removal(const char *dead_ip, int dead_port, const char *reporter_ip);
void get_timestamp(char *buffer);
int find_peer(const char *ip, int port);
int add_peer(const char *ip, int port);
void remove_peer(const char *ip, int port);
VoteRecord* find_vote_record(VoteList *list, const char *peer_id);

// Build timestamp string with millisecond precision
void get_timestamp(char *buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    int millisec = ts.tv_nsec / 1000000;
    
    snprintf(buffer, 64, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec, millisec);
}

// Write logs to stdout and file
void log_msg(const char *msg) {
    char timestamp[64];
    get_timestamp(timestamp);
    
    printf("[%s] %s\n", timestamp, msg);
    fflush(stdout);
    
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, msg);
        fflush(log_file);
    }
}

// Load seed list from config and compute quorum
void load_seeds() {
    FILE *f = fopen("config.txt", "r");
    if (!f) {
        perror("Cannot open config.txt");
        exit(1);
    }
    
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        char ip[16];
        int port;
        if (sscanf(line, "%[^,],%d", ip, &port) == 2) {
            strcpy(seed_list[seed_count].ip, ip);
            seed_list[seed_count].port = port;
            seed_count++;
        }
    }
    fclose(f);
    
    quorum = (seed_count / 2) + 1;
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Loaded %d seeds, quorum: %d", seed_count, quorum);
    log_msg(msg);
}

// Check if peer is already in membership list
int find_peer(const char *ip, int port) {
    pthread_mutex_lock(&peer_list.lock);
    for (int i = 0; i < peer_list.count; i++) {
        if (strcmp(peer_list.peers[i].ip, ip) == 0 && 
            peer_list.peers[i].port == port) {
            pthread_mutex_unlock(&peer_list.lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&peer_list.lock);
    return 0;
}

// Add peer to membership list
int add_peer(const char *ip, int port) {
    pthread_mutex_lock(&peer_list.lock);
    if (peer_list.count < MAX_PEERS) {
        strcpy(peer_list.peers[peer_list.count].ip, ip);
        peer_list.peers[peer_list.count].port = port;
        peer_list.count++;
        pthread_mutex_unlock(&peer_list.lock);
        return 1;
    }
    pthread_mutex_unlock(&peer_list.lock);
    return 0;
}

// Remove peer from membership list
void remove_peer(const char *ip, int port) {
    pthread_mutex_lock(&peer_list.lock);
    for (int i = 0; i < peer_list.count; i++) {
        if (strcmp(peer_list.peers[i].ip, ip) == 0 && 
            peer_list.peers[i].port == port) {
            // Shift remaining peers
            for (int j = i; j < peer_list.count - 1; j++) {
                peer_list.peers[j] = peer_list.peers[j + 1];
            }
            peer_list.count--;
            break;
        }
    }
    pthread_mutex_unlock(&peer_list.lock);
}

// Lookup vote record by peer id
VoteRecord* find_vote_record(VoteList *list, const char *peer_id) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->records[i].peer_id, peer_id) == 0) {
            return &list->records[i];
        }
    }
    return NULL;
}

// Send one request to another seed over TCP
int send_to_seed(const char *ip, int port, const char *message, char *response) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return 0;
    }
    
    send(sock, message, strlen(message), 0);
    int n = recv(sock, response, MAX_BUFFER - 1, 0);
    close(sock);
    
    if (n > 0) {
        response[n] = '\0';
        return 1;
    }
    return 0;
}

// Start registration consensus across seeds
int propose_registration(const char *peer_ip, int peer_port) {
    char peer_id[32];
    snprintf(peer_id, sizeof(peer_id), "%s:%d", peer_ip, peer_port);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Proposing registration for peer %s", peer_id);
    log_msg(msg);
    
    // Count votes from all seeds
    int vote_count = 1; // Our own vote
    
    // Send proposal to other seeds and count responses
    for (int i = 0; i < seed_count; i++) {
        if (seed_list[i].port != seed_port) {
            char proposal[256];
            snprintf(proposal, sizeof(proposal), "VOTE_REGISTER:%s:%d:%d", 
                     peer_ip, peer_port, seed_port);
            
            char response[MAX_BUFFER];
            if (send_to_seed(seed_list[i].ip, seed_list[i].port, proposal, response)) {
                // Count positive responses
                if (strncmp(response, "VOTE_YES", 8) == 0) {
                    vote_count++;
                }
            }
        }
    }
    
    if (vote_count >= quorum) {
        add_peer(peer_ip, peer_port);
        snprintf(msg, sizeof(msg), "CONSENSUS REACHED: Peer %s registered (%d/%d votes)", 
                 peer_id, vote_count, seed_count);
        log_msg(msg);
        return 1;
    } else {
        snprintf(msg, sizeof(msg), "CONSENSUS FAILED: Peer %s registration rejected (%d/%d votes)", 
                 peer_id, vote_count, seed_count);
        log_msg(msg);
        return 0;
    }
}

// Start removal consensus across seeds
int propose_removal(const char *dead_ip, int dead_port, const char *reporter_ip) {
    char peer_id[32];
    snprintf(peer_id, sizeof(peer_id), "%s:%d", dead_ip, dead_port);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Proposing removal for peer %s (reported by %s)", 
             peer_id, reporter_ip);
    log_msg(msg);
    
    // Count votes from all seeds
    int vote_count = 1; // Our own vote
    
    // Send proposal to other seeds and count responses
    for (int i = 0; i < seed_count; i++) {
        if (seed_list[i].port != seed_port) {
            char proposal[256];
            snprintf(proposal, sizeof(proposal), "VOTE_REMOVE:%s:%d:%d", 
                     dead_ip, dead_port, seed_port);
            
            char response[MAX_BUFFER];
            if (send_to_seed(seed_list[i].ip, seed_list[i].port, proposal, response)) {
                // Count positive responses
                if (strncmp(response, "VOTE_YES", 8) == 0) {
                    vote_count++;
                }
            }
        }
    }
    
    if (vote_count >= quorum) {
        remove_peer(dead_ip, dead_port);
        snprintf(msg, sizeof(msg), "CONSENSUS REACHED: Peer %s removed (%d/%d votes)", 
                 peer_id, vote_count, seed_count);
        log_msg(msg);
        return 1;
    } else {
        snprintf(msg, sizeof(msg), "CONSENSUS PENDING: Peer %s removal (%d/%d votes)", 
                 peer_id, vote_count, seed_count);
        log_msg(msg);
        return 0;
    }
}

// Handle one incoming client request
void *handle_client(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[MAX_BUFFER];
    int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    
    if (n > 0) {
        buffer[n] = '\0';
        char msg[512];
        
        // Peer asks to register
        if (strncmp(buffer, "REGISTER:", 9) == 0) {
            char ip[16];
            int port;
            sscanf(buffer, "REGISTER:%[^:]:%d", ip, &port);
            
            snprintf(msg, sizeof(msg), "Registration request from %s:%d", ip, port);
            log_msg(msg);
            
            int success = propose_registration(ip, port);
            
            if (success) {
                send(client_sock, "REGISTERED", 10, 0);
            } else {
                send(client_sock, "REGISTRATION_PENDING", 20, 0);
            }
            
        // Peer asks for membership list
        } else if (strncmp(buffer, "GET_PEERS", 9) == 0) {
            char response[MAX_BUFFER] = "[";
            pthread_mutex_lock(&peer_list.lock);
            
            for (int i = 0; i < peer_list.count; i++) {
                char peer_entry[64];
                snprintf(peer_entry, sizeof(peer_entry), "[\"%s\",%d]%s", 
                         peer_list.peers[i].ip, peer_list.peers[i].port,
                         (i < peer_list.count - 1) ? "," : "");
                strcat(response, peer_entry);
            }
            strcat(response, "]");
            
            snprintf(msg, sizeof(msg), "Sending peer list - %d peers", peer_list.count);
            log_msg(msg);
            
            pthread_mutex_unlock(&peer_list.lock);
            send(client_sock, response, strlen(response), 0);
            
        // Another seed asks for registration vote
        } else if (strncmp(buffer, "VOTE_REGISTER:", 14) == 0) {
            char peer_ip[16];
            int peer_port, proposer_port;
            sscanf(buffer, "VOTE_REGISTER:%[^:]:%d:%d", peer_ip, &peer_port, &proposer_port);
            
            char peer_id[32];
            snprintf(peer_id, sizeof(peer_id), "%s:%d", peer_ip, peer_port);
            
            char msg[256];
            snprintf(msg, sizeof(msg), "Received registration vote request for %s (proposed by seed %d)", 
                     peer_id, proposer_port);
            log_msg(msg);
            
            send(client_sock, "VOTE_YES", 8, 0);
            
        // Another seed asks for removal vote
        } else if (strncmp(buffer, "VOTE_REMOVE:", 12) == 0) {
            char peer_ip[16];
            int peer_port, proposer_port;
            sscanf(buffer, "VOTE_REMOVE:%[^:]:%d:%d", peer_ip, &peer_port, &proposer_port);
            
            char peer_id[32];
            snprintf(peer_id, sizeof(peer_id), "%s:%d", peer_ip, peer_port);
            
            char msg[256];
            snprintf(msg, sizeof(msg), "Received removal vote request for %s (proposed by seed %d)", 
                     peer_id, proposer_port);
            log_msg(msg);
            
            send(client_sock, "VOTE_YES", 8, 0);
            
        // Peer reports a dead node
        } else if (strncmp(buffer, "Dead Node:", 10) == 0) {
            char dead_ip[16], reporter_ip[16];
            int dead_port;
            sscanf(buffer, "Dead Node:%[^:]:%d:%*[^:]:%s", dead_ip, &dead_port, reporter_ip);
            
            snprintf(msg, sizeof(msg), "Dead node report: %s:%d from %s", 
                     dead_ip, dead_port, reporter_ip);
            log_msg(msg);
            
            propose_removal(dead_ip, dead_port, reporter_ip);
            send(client_sock, "REPORT_RECEIVED", 15, 0);
        }
    }
    
    close(client_sock);
    return NULL;
}

// Initialize server socket and accept connections forever
void start_server() {
    pthread_mutex_init(&peer_list.lock, NULL);
    pthread_mutex_init(&registration_votes.lock, NULL);
    pthread_mutex_init(&removal_votes.lock, NULL);
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(seed_port);
    inet_pton(AF_INET, SEED_IP, &addr.sin_addr);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }
    
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Seed running at %s:%d (Quorum: %d/%d)", 
             SEED_IP, seed_port, quorum, seed_count);
    log_msg(msg);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_sock >= 0) {
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, client_sock);
            pthread_detach(thread);
        } else {
            free(client_sock);
        }
    }
}

// Entry point: setup logging, load config, start seed server
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    seed_port = atoi(argv[1]);
    
    // Open log file
    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "output/seed_%d.txt", seed_port);
    log_file = fopen(log_filename, "a");
    
    load_seeds();
    start_server();
    
    if (log_file) fclose(log_file);
    return 0;
}
