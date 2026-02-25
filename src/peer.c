// peer.c
// Peer node implementation with gossip protocol and liveness detection

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>

#define MAX_PEERS 100
#define MAX_SEEDS 10
#define MAX_NEIGHBORS 20
#define MAX_MESSAGES 1000
#define MAX_BUFFER 4096
#define MAX_GOSSIP_MESSAGES 10
#define GOSSIP_INTERVAL 5
#define LIVENESS_INTERVAL 3
#define SUSPICION_THRESHOLD 3

typedef struct {
    char ip[16];
    int port;
} Node;

typedef struct {
    char hash[65];
    char message[256];
    char sender_ip[16];
} MessageRecord;

typedef struct {
    char peer_id[32];
    int count;
} SuspicionRecord;

typedef struct {
    char peer_id[32];
    char reporters[MAX_NEIGHBORS][16];
    int reporter_count;
} DeadReport;

// Global variables
char peer_ip[16] = "127.0.0.1";
int peer_port = 0;

Node seed_list[MAX_SEEDS];
int seed_count = 0;
int quorum = 0;

Node registered_seeds[MAX_SEEDS];
int registered_seed_count = 0;

Node neighbors[MAX_NEIGHBORS];
int neighbor_sockets[MAX_NEIGHBORS];
int neighbor_count = 0;
pthread_mutex_t neighbor_lock;

MessageRecord message_list[MAX_MESSAGES];
int message_count = 0;
pthread_mutex_t ml_lock;

SuspicionRecord suspicions[MAX_NEIGHBORS];
int suspicion_count = 0;
pthread_mutex_t liveness_lock;

DeadReport dead_reports[MAX_NEIGHBORS];
int dead_report_count = 0;

char reported_dead[MAX_NEIGHBORS][32];
int reported_dead_count = 0;

int message_counter = 0;
int running = 1;
FILE *log_file = NULL;

// Function prototypes
void log_msg(const char *msg);
void get_timestamp(char *buffer);
void load_seeds();
int send_to_seed(const char *ip, int port, const char *message, char *response);
int register_with_seeds();
int get_peer_lists(Node *all_peers);
void select_neighbors_powerlaw(Node *peers, int peer_count);
void *peer_listener(void *arg);
void *handle_incoming(void *arg);
void *listen_to_peer(void *arg);
void *generate_gossip(void *arg);
void *check_liveness(void *arg);
void handle_gossip(const char *data, const char *sender_ip);
void forward_gossip(const char *gossip_msg, const char *exclude_ip);
void handle_suspect(const char *data);
void broadcast_suspicion(const char *dead_ip, int dead_port);
void report_dead_to_seeds(const char *dead_ip, int dead_port);
int ping_peer(const char *ip);
void sha256_string(const char *str, char *output);
int find_neighbor_index(const char *ip, int port);
int connect_to_peer(const char *ip, int port);

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

void sha256_string(const char *str, char *output) {
    // Simple hash implementation (not cryptographic)
    unsigned long hash = 5381;
    int c;
    const char *s = str;
    
    while ((c = *s++))
        hash = ((hash << 5) + hash) + c;
    
    snprintf(output, 65, "%016lx", hash);
}

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

int send_to_seed(const char *ip, int port, const char *message, char *response) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    struct timeval tv;
    tv.tv_sec = 5;
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

int register_with_seeds() {
    // Shuffle seeds for random selection
    for (int i = seed_count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Node temp = seed_list[i];
        seed_list[i] = seed_list[j];
        seed_list[j] = temp;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "REGISTER:%s:%d", peer_ip, peer_port);
    
    for (int i = 0; i < seed_count; i++) {
        char response[MAX_BUFFER];
        if (send_to_seed(seed_list[i].ip, seed_list[i].port, msg, response)) {
            if (strstr(response, "REGISTERED")) {
                registered_seeds[registered_seed_count++] = seed_list[i];
                
                char log[256];
                snprintf(log, sizeof(log), "Registered with seed %s:%d", 
                         seed_list[i].ip, seed_list[i].port);
                log_msg(log);
                
                if (registered_seed_count >= quorum) {
                    snprintf(log, sizeof(log), "Successfully registered with %d seeds", 
                             registered_seed_count);
                    log_msg(log);
                    return 1;
                }
            }
        }
    }
    
    if (registered_seed_count >= quorum) {
        return 1;
    } else {
        char log[256];
        snprintf(log, sizeof(log), "ERROR: Could only register with %d seeds, needed %d", 
                 registered_seed_count, quorum);
        log_msg(log);
        return 0;
    }
}

int get_peer_lists(Node *all_peers) {
    int total_peers = 0;
    
    for (int i = 0; i < registered_seed_count; i++) {
        char response[MAX_BUFFER];
        if (send_to_seed(registered_seeds[i].ip, registered_seeds[i].port, 
                         "GET_PEERS", response)) {
            // Parse JSON-like response: [["ip",port],...]
            char *ptr = response;
            while ((ptr = strchr(ptr, '[')) != NULL) {
                ptr++;
                if (*ptr == '"') {
                    char ip[16];
                    int port;
                    if (sscanf(ptr, "\"%[^\"]\",%d", ip, &port) == 2) {
                        // Check if already in list and not self
                        if (!(strcmp(ip, peer_ip) == 0 && port == peer_port)) {
                            int exists = 0;
                            for (int j = 0; j < total_peers; j++) {
                                if (strcmp(all_peers[j].ip, ip) == 0 && 
                                    all_peers[j].port == port) {
                                    exists = 1;
                                    break;
                                }
                            }
                            if (!exists && total_peers < MAX_PEERS) {
                                strcpy(all_peers[total_peers].ip, ip);
                                all_peers[total_peers].port = port;
                                total_peers++;
                            }
                        }
                    }
                }
            }
            
            char log[256];
            snprintf(log, sizeof(log), "Received peers from seed %s:%d", 
                     registered_seeds[i].ip, registered_seeds[i].port);
            log_msg(log);
        }
    }
    
    char log[256];
    snprintf(log, sizeof(log), "Total unique peers discovered: %d", total_peers);
    log_msg(log);
    
    return total_peers;
}

void select_neighbors_powerlaw(Node *peers, int peer_count) {
    if (peer_count == 0) {
        log_msg("No peers available for connection");
        return;
    }
    
    int num_neighbors;
    if (peer_count <= 3) {
        num_neighbors = peer_count;
    } else {
        // Power-law: prefer lower degrees
        // Generate number between 2 and min(peer_count, 5)
        int max_degree = (peer_count < 6) ? peer_count : 5;
        double r = (double)rand() / RAND_MAX;
        // Inverse power-law: P(k) ~ k^(-1.5)
        num_neighbors = 2 + (int)(r * r * (max_degree - 2));
        if (num_neighbors > max_degree) num_neighbors = max_degree;
    }
    
    // Randomly select neighbors
    for (int i = peer_count - 1; i > 0 && i >= peer_count - num_neighbors; i--) {
        int j = rand() % (i + 1);
        Node temp = peers[i];
        peers[i] = peers[j];
        peers[j] = temp;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Selected %d neighbors using power-law distribution", 
             num_neighbors);
    log_msg(msg);
    
    // Connect to selected neighbors
    for (int i = peer_count - num_neighbors; i < peer_count; i++) {
        connect_to_peer(peers[i].ip, peers[i].port);
    }
}

int connect_to_peer(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to connect to %s:%d", ip, port);
        log_msg(msg);
        return 0;
    }
    
    // Send HELLO message
    char hello[256];
    snprintf(hello, sizeof(hello), "HELLO:%s:%d", peer_ip, peer_port);
    send(sock, hello, strlen(hello), 0);
    
    pthread_mutex_lock(&neighbor_lock);
    if (neighbor_count < MAX_NEIGHBORS) {
        strcpy(neighbors[neighbor_count].ip, ip);
        neighbors[neighbor_count].port = port;
        neighbor_sockets[neighbor_count] = sock;
        neighbor_count++;
    }
    pthread_mutex_unlock(&neighbor_lock);
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Connected to peer %s:%d", ip, port);
    log_msg(msg);
    
    // Start thread to listen to this peer
    pthread_t thread;
    int *sock_ptr = malloc(sizeof(int));
    *sock_ptr = sock;
    pthread_create(&thread, NULL, listen_to_peer, sock_ptr);
    pthread_detach(thread);
    
    return 1;
}

int find_neighbor_index(const char *ip, int port) {
    for (int i = 0; i < neighbor_count; i++) {
        if (strcmp(neighbors[i].ip, ip) == 0 && neighbors[i].port == port) {
            return i;
        }
    }
    return -1;
}

void *listen_to_peer(void *arg) {
    int sock = *(int*)arg;
    free(arg);
    
    char buffer[MAX_BUFFER];
    while (running) {
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        
        buffer[n] = '\0';
        
        // Find sender IP from socket
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        getpeername(sock, (struct sockaddr*)&addr, &addr_len);
        char sender_ip[16];
        inet_ntop(AF_INET, &addr.sin_addr, sender_ip, sizeof(sender_ip));
        
        if (strncmp(buffer, "GOSSIP:", 7) == 0) {
            handle_gossip(buffer, sender_ip);
        } else if (strncmp(buffer, "SUSPECT:", 8) == 0) {
            handle_suspect(buffer);
        } else if (strncmp(buffer, "PING", 4) == 0) {
            send(sock, "PONG", 4, 0);
        }
    }
    
    close(sock);
    return NULL;
}

void handle_gossip(const char *data, const char *sender_ip) {
    // Format: GOSSIP:<timestamp>:<ip>:<msg#>
    char message[256];
    sscanf(data, "GOSSIP:%[^\n]", message);
    
    char hash[65];
    sha256_string(message, hash);
    
    pthread_mutex_lock(&ml_lock);
    int found = 0;
    for (int i = 0; i < message_count; i++) {
        if (strcmp(message_list[i].hash, hash) == 0) {
            found = 1;
            break;
        }
    }
    
    if (!found && message_count < MAX_MESSAGES) {
        strcpy(message_list[message_count].hash, hash);
        strcpy(message_list[message_count].message, message);
        strcpy(message_list[message_count].sender_ip, sender_ip);
        message_count++;
        
        char log[512];
        snprintf(log, sizeof(log), "GOSSIP RECEIVED: %s (from %s)", message, sender_ip);
        log_msg(log);
        
        pthread_mutex_unlock(&ml_lock);
        
        // Forward to all neighbors except sender
        forward_gossip(data, sender_ip);
    } else {
        pthread_mutex_unlock(&ml_lock);
    }
}

void forward_gossip(const char *gossip_msg, const char *exclude_ip) {
    pthread_mutex_lock(&neighbor_lock);
    for (int i = 0; i < neighbor_count; i++) {
        if (strcmp(neighbors[i].ip, exclude_ip) != 0) {
            send(neighbor_sockets[i], gossip_msg, strlen(gossip_msg), MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&neighbor_lock);
}

void handle_suspect(const char *data) {
    // Format: SUSPECT:<dead_ip>:<dead_port>:<reporter_ip>
    char dead_ip[16], reporter_ip[16];
    int dead_port;
    sscanf(data, "SUSPECT:%[^:]:%d:%s", dead_ip, &dead_port, reporter_ip);
    
    char peer_id[32];
    snprintf(peer_id, sizeof(peer_id), "%s:%d", dead_ip, dead_port);
    
    pthread_mutex_lock(&liveness_lock);
    
    // Find or create dead report
    DeadReport *report = NULL;
    for (int i = 0; i < dead_report_count; i++) {
        if (strcmp(dead_reports[i].peer_id, peer_id) == 0) {
            report = &dead_reports[i];
            break;
        }
    }
    
    if (!report && dead_report_count < MAX_NEIGHBORS) {
        report = &dead_reports[dead_report_count++];
        strcpy(report->peer_id, peer_id);
        report->reporter_count = 0;
    }
    
    if (report) {
        // Add reporter if not already present
        int found = 0;
        for (int i = 0; i < report->reporter_count; i++) {
            if (strcmp(report->reporters[i], reporter_ip) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && report->reporter_count < MAX_NEIGHBORS) {
            strcpy(report->reporters[report->reporter_count++], reporter_ip);
        }
        
        // Check peer consensus
        int neighbor_quorum = (neighbor_count / 2) + 1;
        if (report->reporter_count >= neighbor_quorum && neighbor_count > 0) {
            // Check if already reported
            int already_reported = 0;
            for (int i = 0; i < reported_dead_count; i++) {
                if (strcmp(reported_dead[i], peer_id) == 0) {
                    already_reported = 1;
                    break;
                }
            }
            
            if (!already_reported) {
                char msg[256];
                snprintf(msg, sizeof(msg), "PEER CONSENSUS: %s confirmed dead by %d peers", 
                         peer_id, report->reporter_count);
                log_msg(msg);
                
                pthread_mutex_unlock(&liveness_lock);
                report_dead_to_seeds(dead_ip, dead_port);
                
                pthread_mutex_lock(&liveness_lock);
                strcpy(reported_dead[reported_dead_count++], peer_id);
            }
        }
    }
    
    pthread_mutex_unlock(&liveness_lock);
}

void broadcast_suspicion(const char *dead_ip, int dead_port) {
    char suspect_msg[256];
    snprintf(suspect_msg, sizeof(suspect_msg), "SUSPECT:%s:%d:%s", 
             dead_ip, dead_port, peer_ip);
    
    pthread_mutex_lock(&neighbor_lock);
    for (int i = 0; i < neighbor_count; i++) {
        if (strcmp(neighbors[i].ip, dead_ip) != 0 || neighbors[i].port != dead_port) {
            send(neighbor_sockets[i], suspect_msg, strlen(suspect_msg), MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&neighbor_lock);
    
    // Add own suspicion
    char peer_id[32];
    snprintf(peer_id, sizeof(peer_id), "%s:%d", dead_ip, dead_port);
    
    pthread_mutex_lock(&liveness_lock);
    DeadReport *report = NULL;
    for (int i = 0; i < dead_report_count; i++) {
        if (strcmp(dead_reports[i].peer_id, peer_id) == 0) {
            report = &dead_reports[i];
            break;
        }
    }
    
    if (!report && dead_report_count < MAX_NEIGHBORS) {
        report = &dead_reports[dead_report_count++];
        strcpy(report->peer_id, peer_id);
        report->reporter_count = 0;
    }
    
    if (report) {
        strcpy(report->reporters[report->reporter_count++], peer_ip);
        
        int neighbor_quorum = (neighbor_count / 2) + 1;
        if (report->reporter_count >= neighbor_quorum && neighbor_count > 0) {
            int already_reported = 0;
            for (int i = 0; i < reported_dead_count; i++) {
                if (strcmp(reported_dead[i], peer_id) == 0) {
                    already_reported = 1;
                    break;
                }
            }
            
            if (!already_reported) {
                char msg[256];
                snprintf(msg, sizeof(msg), "PEER CONSENSUS: %s confirmed dead by %d peers", 
                         peer_id, report->reporter_count);
                log_msg(msg);
                
                pthread_mutex_unlock(&liveness_lock);
                report_dead_to_seeds(dead_ip, dead_port);
                
                pthread_mutex_lock(&liveness_lock);
                strcpy(reported_dead[reported_dead_count++], peer_id);
            }
        }
    }
    pthread_mutex_unlock(&liveness_lock);
}

void report_dead_to_seeds(const char *dead_ip, int dead_port) {
    char timestamp[64];
    get_timestamp(timestamp);
    
    char msg[512];
    snprintf(msg, sizeof(msg), "Dead Node:%s:%d:%s:%s", 
             dead_ip, dead_port, timestamp, peer_ip);
    
    char log[256];
    snprintf(log, sizeof(log), "REPORTING to seeds: Dead node %s:%d", dead_ip, dead_port);
    log_msg(log);
    
    for (int i = 0; i < registered_seed_count; i++) {
        char response[MAX_BUFFER];
        if (send_to_seed(registered_seeds[i].ip, registered_seeds[i].port, 
                         msg, response)) {
            snprintf(log, sizeof(log), "Dead node report acknowledged by seed %s:%d", 
                     registered_seeds[i].ip, registered_seeds[i].port);
            log_msg(log);
        }
    }
}

int ping_peer(const char *ip) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s > /dev/null 2>&1", ip);
    return (system(cmd) == 0);
}

void *check_liveness(void *arg) {
    sleep(5); // Wait for connections
    
    while (running) {
        sleep(LIVENESS_INTERVAL);
        
        pthread_mutex_lock(&neighbor_lock);
        Node neighbors_copy[MAX_NEIGHBORS];
        int count = neighbor_count;
        memcpy(neighbors_copy, neighbors, sizeof(Node) * count);
        pthread_mutex_unlock(&neighbor_lock);
        
        for (int i = 0; i < count; i++) {
            char peer_id[32];
            snprintf(peer_id, sizeof(peer_id), "%s:%d", 
                     neighbors_copy[i].ip, neighbors_copy[i].port);
            
            int is_alive = ping_peer(neighbors_copy[i].ip);
            
            pthread_mutex_lock(&liveness_lock);
            
            if (!is_alive) {
                // Find or create suspicion record
                SuspicionRecord *rec = NULL;
                for (int j = 0; j < suspicion_count; j++) {
                    if (strcmp(suspicions[j].peer_id, peer_id) == 0) {
                        rec = &suspicions[j];
                        break;
                    }
                }
                
                if (!rec && suspicion_count < MAX_NEIGHBORS) {
                    rec = &suspicions[suspicion_count++];
                    strcpy(rec->peer_id, peer_id);
                    rec->count = 0;
                }
                
                if (rec) {
                    rec->count++;
                    
                    if (rec->count >= SUSPICION_THRESHOLD) {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "SUSPECTING peer %s (failed %d pings)", 
                                 peer_id, rec->count);
                        log_msg(msg);
                        
                        pthread_mutex_unlock(&liveness_lock);
                        broadcast_suspicion(neighbors_copy[i].ip, neighbors_copy[i].port);
                        pthread_mutex_lock(&liveness_lock);
                    }
                }
            } else {
                // Reset suspicion
                for (int j = 0; j < suspicion_count; j++) {
                    if (strcmp(suspicions[j].peer_id, peer_id) == 0) {
                        suspicions[j].count = 0;
                        break;
                    }
                }
            }
            
            pthread_mutex_unlock(&liveness_lock);
        }
    }
    
    return NULL;
}

void *generate_gossip(void *arg) {
    sleep(2); // Wait for network to stabilize
    
    while (message_counter < MAX_GOSSIP_MESSAGES && running) {
        char timestamp[64];
        get_timestamp(timestamp);
        
        char message[256];
        snprintf(message, sizeof(message), "%s:%s:%d", timestamp, peer_ip, message_counter);
        
        char gossip_msg[512];
        snprintf(gossip_msg, sizeof(gossip_msg), "GOSSIP:%s", message);
        
        message_counter++;
        
        // Add to own message list
        char hash[65];
        sha256_string(message, hash);
        
        pthread_mutex_lock(&ml_lock);
        if (message_count < MAX_MESSAGES) {
            strcpy(message_list[message_count].hash, hash);
            strcpy(message_list[message_count].message, message);
            strcpy(message_list[message_count].sender_ip, peer_ip);
            message_count++;
        }
        pthread_mutex_unlock(&ml_lock);
        
        char log[512];
        snprintf(log, sizeof(log), "GOSSIP GENERATED: %s", message);
        log_msg(log);
        
        // Send to all neighbors
        pthread_mutex_lock(&neighbor_lock);
        for (int i = 0; i < neighbor_count; i++) {
            send(neighbor_sockets[i], gossip_msg, strlen(gossip_msg), MSG_NOSIGNAL);
        }
        pthread_mutex_unlock(&neighbor_lock);
        
        sleep(GOSSIP_INTERVAL);
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Finished generating %d gossip messages", message_counter);
    log_msg(msg);
    
    return NULL;
}

void *handle_incoming(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[MAX_BUFFER];
    int n = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    
    if (n > 0) {
        buffer[n] = '\0';
        
        if (strncmp(buffer, "HELLO:", 6) == 0) {
            char ip[16];
            int port;
            sscanf(buffer, "HELLO:%[^:]:%d", ip, &port);
            
            char msg[256];
            snprintf(msg, sizeof(msg), "Incoming connection from %s:%d", ip, port);
            log_msg(msg);
            
            pthread_mutex_lock(&neighbor_lock);
            if (neighbor_count < MAX_NEIGHBORS) {
                strcpy(neighbors[neighbor_count].ip, ip);
                neighbors[neighbor_count].port = port;
                neighbor_sockets[neighbor_count] = client_sock;
                neighbor_count++;
            }
            pthread_mutex_unlock(&neighbor_lock);
            
            // Start listening thread
            pthread_t thread;
            int *sock_ptr = malloc(sizeof(int));
            *sock_ptr = client_sock;
            pthread_create(&thread, NULL, listen_to_peer, sock_ptr);
            pthread_detach(thread);
            
            return NULL; // Don't close socket
        }
    }
    
    close(client_sock);
    return NULL;
}

void *peer_listener(void *arg) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip, &addr.sin_addr);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        return NULL;
    }
    
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        return NULL;
    }
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Peer server started at %s:%d", peer_ip, peer_port);
    log_msg(msg);
    
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_sock >= 0) {
            pthread_t thread;
            pthread_create(&thread, NULL, handle_incoming, client_sock);
            pthread_detach(thread);
        } else {
            free(client_sock);
        }
    }
    
    close(server_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    srand(time(NULL));
    peer_port = atoi(argv[1]);
    
    // Open log file
    char log_filename[64];
    snprintf(log_filename, sizeof(log_filename), "output/peer_%d.txt", peer_port);
    log_file = fopen(log_filename, "a");
    
    char msg[256];
    snprintf(msg, sizeof(msg), "Peer starting at %s:%d", peer_ip, peer_port);
    log_msg(msg);
    
    // Initialize mutexes
    pthread_mutex_init(&neighbor_lock, NULL);
    pthread_mutex_init(&ml_lock, NULL);
    pthread_mutex_init(&liveness_lock, NULL);
    
    // Load seeds
    load_seeds();
    
    // Start server thread
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, peer_listener, NULL);
    pthread_detach(server_thread);
    
    sleep(1);
    
    // Register with seeds
    if (!register_with_seeds()) {
        log_msg("Failed to register with sufficient seeds. Exiting.");
        running = 0;
        if (log_file) fclose(log_file);
        return 1;
    }
    
    // Get peer lists
    Node all_peers[MAX_PEERS];
    int peer_count = get_peer_lists(all_peers);
    
    // Select and connect to neighbors
    select_neighbors_powerlaw(all_peers, peer_count);
    
    sleep(1);
    snprintf(msg, sizeof(msg), "Connected to %d neighbors", neighbor_count);
    log_msg(msg);
    
    // Start gossip generation
    pthread_t gossip_thread;
    pthread_create(&gossip_thread, NULL, generate_gossip, NULL);
    pthread_detach(gossip_thread);
    
    // Start liveness checking
    pthread_t liveness_thread;
    pthread_create(&liveness_thread, NULL, check_liveness, NULL);
    pthread_detach(liveness_thread);
    
    // Keep running
    while (running) {
        sleep(1);
    }
    
    if (log_file) fclose(log_file);
    return 0;
}
