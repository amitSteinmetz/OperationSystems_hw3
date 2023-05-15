#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <sys/un.h>
#include "chatLogo.h"

#define BUFFER_SIZE 1024
#define DATA_SIZE 104857600 // 100MB chunk size
#define SOCKET_PATH "/tmp/uds_datagram_server"

void send_TCP(int sockfd, char* data, int quiet_mode);
void receive_TCP(int sockfd,int expected_size, int original_checksum, int quiet_mode);
void generate_data(char* data);
int generate_checksum(char* data);
int open_server_TCP(int port, char* ip_version, int quiet_mode);
int connect_to_server_TCP(char *ip, int port, char* ip_version, int quiet_mode);
void chat(int sockfd);
int open_server_UDP_andRecv(int port, char* ip_version, int expected_size, int quiet_mode);
int connect_to_server_UDP_andSend(int port, char* ip_version, char* data, int quiet_mode);
int connect_to_server_UDS(int port, char* type, int quiet_mode, int dgram, char *data);
int open_server_UDS(int port, char* type, int quiet_mode, int dgram, int expected_size);

int main(int argc, char *argv[])
{
    /* Valid number of argumants check */
    if (argc < 3 || argc > 8) {
        fprintf(stderr, "Usage: %s -c/-s IP(optional) PORT -p/-q(optional) <type>(optional) <param>(optional)\n", argv[0]);
        return 1;
    }

    int is_server = 0;
    int port;
    char *ip;
    int sockfd, quiet_mode = 0;

    /* Who are you - server or client */
    if ((strcmp(argv[1], "-c") == 0) && ((argc == 4) || (argc == 7) || (argc == 8))) {
        ip = argv[2];
        port = atoi(argv[3]); // "atoi" - Convert a string to an integer.
    } else if ((strcmp(argv[1], "-s") == 0) && ((argc == 3) || (argc == 6) || (argc == 7))) {
        is_server = 1;
        port = atoi(argv[2]);
    } else {
        fprintf(stderr, "Usage: %s -c/-s IP(optional) PORT -p/-q(optional) <type>(optional) <param>(optional)\n", argv[0]);
        return 1;
    }
    
    ///////////////////////////////////////////////////// PERFORMANCE TEST
    if (argc > 4)
    {
        /* Quiet mode or loud mode */
        if (((strcmp(argv[3], "-q") == 0) || (strcmp(argv[4], "-q") == 0) || (strcmp(argv[5], "-q") == 0)) 
        && ((argc == 6) || (argc == 7) || (argc == 8))) {
            quiet_mode = 1;
        }

        /* Generate 100mb data */
        char* data = (char*) malloc(DATA_SIZE);
        generate_data(data);
        
        /* Generate checksum */
        char* chunk = (char*) malloc(DATA_SIZE + sizeof(int));
        int checksum = generate_checksum(data);
        memcpy(chunk, data, DATA_SIZE);
        memcpy(chunk + strlen(data), &checksum, sizeof(int));
        if (!is_server && !quiet_mode) printf("Checksum value when sending: %d\nGenerated data amount icluding checksum: %lu\n", checksum, strlen(chunk));

        
        /* Start measure the time */
        clock_t measure_time;
        measure_time = clock();

        /* Create rhe requested communication style, and send the data through it */
        if((strcmp(argv[argc-1], "tcp") == 0))
        {
            /* Create the socket TCP connection */
            if (is_server) sockfd = open_server_TCP(port, argv[argc-2], quiet_mode);
            else sockfd = connect_to_server_TCP(ip, port, argv[argc-2], quiet_mode);
            
            if (sockfd < 0) {
                fprintf(stderr, "Failed to create socket\n");
                return 1;
            }

            /* Send and receive the chunk */
            if(!is_server) send_TCP(sockfd, chunk, quiet_mode);
            else receive_TCP(sockfd, strlen(chunk), checksum, quiet_mode); 
        }
        else if ((strcmp(argv[argc-1], "udp") == 0))
        {
            /* Create the socket UDP connection, then send and receive the chunk */
            if (is_server) sockfd = open_server_UDP_andRecv(port, argv[argc-2], strlen(chunk), quiet_mode);
            else sockfd = connect_to_server_UDP_andSend(port, argv[argc-2], chunk, quiet_mode); 

            if (sockfd < 0) {
                fprintf(stderr, "Failed to create socket\n");
                return 1;
            }
        }
        else if ((strcmp(argv[argc-2], "uds") == 0))
        {
            int dgram = 0;
            
            if ((strcmp(argv[argc-1], "datagram") == 0)) dgram = 1;
            /* Create the socket UDP connection */
            if (is_server) sockfd = open_server_UDS(port, argv[argc-1], quiet_mode, dgram, strlen(chunk));
            else sockfd = connect_to_server_UDS(port, argv[argc-1], quiet_mode, dgram, chunk); 

        }
        else if ((strcmp(argv[argc-1], "mmap") == 0))
        {

        }
         else if ((strcmp(argv[argc-1], "pipe") == 0))
        {

        }
        else // Wrong type
        {
            fprintf(stderr, "Usage: %s -c/-s IP(optional) PORT -p/-q(optional) <type>(optional) <param>(optional)\n", argv[0]);
            return 1;
        }

        /* Times up */
        measure_time = clock() - measure_time; // The calculation idea: end_time - start_ time = work_time
        double time_taken = (((double) measure_time) / CLOCKS_PER_SEC);
        int time_ms = (time_taken * 10000);
        if (is_server) printf("Time taken: %d\n", time_ms); // time is shown on server side only
    }

    /////////////////////////////////////////////////////

    ///////////////////////////////////////////////////// CHAT TOOL
    else
    {
        /* Create the socket TCP connection */
    if (is_server) {
        sockfd = open_server_TCP(port, "ipv4", quiet_mode);
    } else {
        sockfd = connect_to_server_TCP(ip, port, "ipv4", quiet_mode);
    }

    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 1;
    }

    /* Start conversation */
    printf("%s", logo);
    chat(sockfd);

    close(sockfd);
    }

    ///////////////////////////////////////////////////
}

void chat(int sockfd)
{
    // I chose to use poll()
    struct pollfd pollfds[2];
    char buffer[BUFFER_SIZE];
    int n;

    // Set up pollfd for stdin
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN;

    // Set up pollfd for sockfd
    pollfds[1].fd = sockfd;
    pollfds[1].events = POLLIN;

    while (1) 
    {
        // Call poll - with the array of sockets, and their number, with no timeout
        if (poll(pollfds, 2, -1) == -1) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        // Check if stdin has input
        if (pollfds[0].revents & POLLIN) {
            // Read input from stdin
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                perror("fgets");
                exit(EXIT_FAILURE);
            }

            if (strcmp("quit\n", buffer) == 0)
            {
                printf("[SYSTEM] Exiting conversation...\n");
                send(sockfd, "quit", strlen(buffer), 0);
                close(sockfd);
                return;
            }
            
            // Send input to sockfd
            if (send(sockfd, buffer, strlen(buffer), 0) == -1) {
                perror("send");
                exit(EXIT_FAILURE);
            }
           
        }

        // Check if sockfd has input
        if (pollfds[1].revents & POLLIN) {
            // Read input from sockfd
            if ((n = recv(sockfd, buffer, BUFFER_SIZE-1, 0)) == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }
            
            // Print received input
            buffer[n] = '\0';
            if (strcmp("quit", buffer) == 0)
            {
                printf("[SYSTEM] Friend quited, Exiting conversation...\n");
                close(sockfd);
                return;
            } 
            printf("[Friend] %s", buffer);
        }
    }
}

int open_server_TCP(int port, char* ip_version, int quiet_mode) 
{
    int sockfd;

    struct sockaddr_in6 server_addr6;
    struct sockaddr_in server_addr4;

    /* Create first socket */
    
    if (strcmp("ipv4", ip_version) == 0) 
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) return -1;

        /* Bind (with preparations) */
        memset(&server_addr4, 0, sizeof(server_addr4));
        server_addr4.sin_family = AF_INET;
        server_addr4.sin_addr.s_addr = INADDR_ANY;
        server_addr4.sin_port = htons(port);
        
        if (bind(sockfd, (struct sockaddr *) &server_addr4, sizeof(server_addr4)) < 0) 
        {
            close(sockfd);
            return -1;
        }
    }
    else 
    {
        sockfd = socket(AF_INET6, SOCK_STREAM, 0);
        if (sockfd < 0) return -1;

        /* Bind (with preparations) */
        memset(&server_addr6, 0, sizeof(server_addr6));
        server_addr6.sin6_family = AF_INET6;
        server_addr6.sin6_port = htons(port);
        
        if (bind(sockfd, (struct sockaddr *) &server_addr6, sizeof(server_addr6)) < 0) 
        {
            close(sockfd);
            return -1;
        }
    }
    
    // Listen to incoming connection requests
      if ((listen(sockfd, 1)) != 0) {
        printf("Server has failed to listen\n");
        return -1;
    } else {
        if (!quiet_mode) printf("Listening to incoming connection requests\n");
    }

    /* Accept the connection request */
    struct sockaddr_in senderAddress;
    socklen_t sender_address_length = sizeof(senderAddress);

    int finalSock = accept(sockfd, (struct sockaddr *) &senderAddress, &sender_address_length);

    if (finalSock  == -1) {
        printf("Connection has failed\n");
        return -1;
    } else {
        if (!quiet_mode) printf("Connection with client accepted\n");
    }

    return finalSock ;
}

int connect_to_server_TCP(char* ip, int port, char* ip_version, int quiet_mode)
{
    int sockfd;
    struct sockaddr_in6 server_addr6;
    struct sockaddr_in server_addr4;

    /* Creating socket */
    if (strcmp("ipv4", ip_version) == 0)
    {
         if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        {
        perror("socket creation failed");
        return -1;
        }

        /* Filling server information */
        memset(&server_addr4, 0, sizeof(server_addr4));
        server_addr4.sin_family = AF_INET;
        server_addr4.sin_port = htons(port);

        /* Convert IPv4 and IPv6 addresses from text to binary form */
        if(inet_pton(AF_INET, ip, &server_addr4.sin_addr)<=0)
        {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
        }

        /* Connect the socket with the server */
        if (connect(sockfd, (struct sockaddr *)&server_addr4, sizeof(server_addr4)) < 0) 
        {
            perror("connection with the server failed");
            return -1;
        }
    }
    else
    {
        if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) 
        {
        perror("socket creation failed");
        return -1;
        }

        /* Filling server information */
        memset(&server_addr6, 0, sizeof(server_addr6));
        server_addr6.sin6_family = AF_INET6;
        server_addr6.sin6_port = htons(port);

        /* Convert IPv4 and IPv6 addresses from text to binary form */
        if(inet_pton(AF_INET6, ip, &server_addr6.sin6_addr)<=0)
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }

        /* Connect the socket with the server */
        if (connect(sockfd, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) < 0) 
        {
            perror("connection with the server failed");
            return -1;
        }
    }

    if (!quiet_mode) printf("Connected to server\n");
    
    return sockfd;
}

void generate_data(char* data) {
    for (int i = 0; i < DATA_SIZE; i++) {
        data[i] = 10; // fill with meaningless number
    }
    data[DATA_SIZE - 1] = '\0'; // add null terminator
}

int generate_checksum(char* data)
{
    unsigned long int count = 0;
    for (int i = 0; i < 1024; i++)  count += data[i]; 
    return (count % 1024) + 1;
    
}

void send_TCP(int sockfd, char* data, int quiet_mode)
{
    size_t chunk_size = 1024;
    size_t num_chunks = strlen(data) / chunk_size;
    size_t last_chunk_size = strlen(data) % chunk_size;

    // Send full chunks
    for (size_t i = 0; i < num_chunks; i++) {
        if (send(sockfd, data + i * chunk_size, chunk_size, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }

    // Send last chunk
    if (last_chunk_size > 0) {
        if (send(sockfd, data + num_chunks * chunk_size, last_chunk_size, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
        if (!quiet_mode) printf("Data Sent amount: %lu\n", strlen(data));
    }

void receive_TCP(int sockfd,int expected_size, int original_checksum, int quiet_mode) {

    int total_bytes_received = 0;
    char* recvBuf = (char*) malloc(expected_size);
    
    while(1) {
        
        int chunky = recv(sockfd, recvBuf + total_bytes_received, 1024, 0);
        total_bytes_received += chunky;
        if (chunky == -1) {
            printf("Server has failed to receive the file\n");
            return;
        }
        if (total_bytes_received == expected_size) break;
    }
    if (!quiet_mode) printf("Data received amount: %d\n", total_bytes_received);

    // Calculate and Extract checksum
    int calculated_checksum = generate_checksum(recvBuf);
    if (!quiet_mode) printf("Checksum value when receiving: %d\n", calculated_checksum);

    if (calculated_checksum == original_checksum) {
        if (!quiet_mode) printf("Good checksum!\n");
    } else {
        if (!quiet_mode) printf("Bad checksum!\n");
    }
}

int open_server_UDP_andRecv(int port, char* ip_version, int expected_size, int quiet_mode)
{
    int sockfd;
    unsigned int len;
    struct sockaddr_in server_addr4, client_addr4;
    struct sockaddr_in6 server_addr6, client_addr6;
    int total_bytes_received = 0;
    char* recvBuf = (char*) malloc(expected_size);
    struct timeval tv;
    tv.tv_sec = 7;
    tv.tv_usec = 0;

    /* Creating socket and bind, then receive the data */
    if (strcmp("ipv4", ip_version) == 0) 
    {
        // Create
         if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }
        
        // Bind
        memset(&server_addr4, 0, sizeof(server_addr4));
        memset(&client_addr4, 0, sizeof(client_addr4));
        
        server_addr4.sin_family = AF_INET;
        server_addr4.sin_addr.s_addr = INADDR_ANY;
        server_addr4.sin_port = htons(port);

        if (bind(sockfd, (const struct sockaddr *)&server_addr4, sizeof(server_addr4)) < 0) 
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }
        
        // Receive
        len = sizeof(client_addr4);
        
        while (1)
        {
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            int chunky = recvfrom(sockfd, (char *)recvBuf, BUFFER_SIZE,
             MSG_WAITALL, (struct sockaddr *)&client_addr4, &len);
            total_bytes_received += chunky;
            
            if (chunky < 0 && errno == EWOULDBLOCK) {
                // timeout occurred
                break;
            } else if (chunky < 0) {
                // error occurred
                printf("Server has failed to receive the current chunk\n");
                return -1;
            } 
        }
    }
    else
    {
        // Create
         if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Bind
        memset(&server_addr6, 0, sizeof(server_addr6));
        memset(&client_addr6, 0, sizeof(client_addr6));
        server_addr6.sin6_family = AF_INET6;
        server_addr6.sin6_port = htons(port);

        if (bind(sockfd, (const struct sockaddr *)&server_addr6, sizeof(server_addr6)) < 0) 
        {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        // Receive
        len = sizeof(client_addr4);

        while (1)
        {
             setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
            int chunky = recvfrom(sockfd, (char *)recvBuf, BUFFER_SIZE,
             MSG_WAITALL, (struct sockaddr *)&client_addr6, &len);
            total_bytes_received += chunky;
             
            if (chunky < 0 && errno == EWOULDBLOCK) {
                // timeout occurred
                break;
            } else if (chunky < 0) {
                // error occurred
                printf("Server has failed to receive the current chunk\n");
                return -1;
            } 
        }
    }

    if (!quiet_mode) printf("Open server UDP went succesfully!\n");
   
    return sockfd;
}

int connect_to_server_UDP_andSend(int port, char* ip_version, char *data, int quiet_mode)
 {
    int sockfd;
    struct sockaddr_in6 server_addr6;
    struct sockaddr_in server_addr4;
    size_t chunk_size = BUFFER_SIZE;
    size_t num_chunks = strlen(data) / chunk_size;
    size_t last_chunk_size = strlen(data) % chunk_size;

    /* Creating socket */
    if (strcmp("ipv4", ip_version) == 0) 
    {
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        /* Send the chunk */

        // preparations before sending
        memset(&server_addr4, 0, sizeof(server_addr4));
        server_addr4.sin_family = AF_INET;
        server_addr4.sin_port = htons(port);
        server_addr4.sin_addr.s_addr = INADDR_ANY;
       
        // Send full chunks
        for (size_t i = 0; i < num_chunks; i++)
        {
            sendto(sockfd, data + i * chunk_size, chunk_size, MSG_CONFIRM, 
            (const struct sockaddr *)&server_addr4, sizeof(server_addr4));
        }

        // Send last chunk
        if (last_chunk_size > 0)
        {
            sendto(sockfd, data + num_chunks * chunk_size, last_chunk_size, MSG_CONFIRM, 
            (const struct sockaddr *)&server_addr4, sizeof(server_addr4));
        }
    }
    else
    {
        if ((sockfd = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
        {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        /* Send the chunk */

        // preparations before sending
        memset(&server_addr6, 0, sizeof(server_addr6));
        server_addr6.sin6_family = AF_INET6;
        server_addr6.sin6_port = htons(port);

        // Send full chunks
        for (size_t i = 0; i < num_chunks; i++)
        {
            sendto(sockfd, data + i * chunk_size, chunk_size, MSG_CONFIRM, 
            (const struct sockaddr *)&server_addr6, sizeof(server_addr6));
        }

        // Send last chunk
        if (last_chunk_size > 0)
        {
            sendto(sockfd, data + num_chunks * chunk_size, last_chunk_size, MSG_CONFIRM, 
            (const struct sockaddr *)&server_addr6, sizeof(server_addr6));
        }
    }

    if (!quiet_mode) printf("Creating socket UDP went succesfully!\n");

    return sockfd;
 }

int open_server_UDS(int port, char* type, int quiet_mode, int dgram, int expected_size)
{
    struct sockaddr_un server_addr, client_addr;
    char buf[1024];
    int server_sockfd, client_sockfd, chunky;
    socklen_t client_len;
    int total_bytes_received = 0;
    char* recvBuf = (char*) malloc(expected_size);
    
    /* Create a socket for the server */
    if (dgram) server_sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    else server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    if (server_sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Bind the socket to a file path */ 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    
    /* Listen */
    if (!dgram)
    {
        if (listen(server_sockfd, 5) == -1)
        {
            perror("listen() error");
            exit(EXIT_FAILURE);
        }
        
        /* Accept */
        client_len = sizeof(client_addr);
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd == -1) {
            perror("accept() error");
            exit(EXIT_FAILURE);
        }
        
        if (!quiet_mode) printf("Connection with client accepted!\n");
    }
    
    /* Receive the data */
    if (!dgram)
    {
        while(1)
        {
            chunky = read(client_sockfd, recvBuf + total_bytes_received, 1024);
            total_bytes_received += chunky;
            if (chunky == -1) {
                printf("Server has failed to receive the file\n");
                return -1;
            }
            if (total_bytes_received == expected_size) break;
        }
    }
    else 
    {
        while (1)
        {
        // Receive data from the client
        memset(buf, 0, sizeof(buf));
        client_len = sizeof(client_addr);

        if ((chunky = recvfrom(server_sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &client_len)) == -1) {
            perror("recvfrom");
            exit(EXIT_FAILURE);
        }
        total_bytes_received += chunky;
        }
    }

    if (!quiet_mode) printf("Data received amount: %d\n", total_bytes_received);
    
    return server_sockfd;
}

int connect_to_server_UDS(int port, char* type, int quiet_mode, int dgram, char *data)
{
    int client_fd;
    size_t chunk_size = BUFFER_SIZE;
    size_t num_chunks = strlen(data) / chunk_size;
    size_t last_chunk_size = strlen(data) % chunk_size;
    struct sockaddr_un server_addr;
    

    if (dgram) client_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    else client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    if (client_fd < 0) {
        perror("socket() error");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect() error");
        exit(EXIT_FAILURE);
    }

    if (!quiet_mode)printf("Connected to server!\n");

    /* Send the data */
   // Send full chunks
    for (size_t i = 0; i < num_chunks; i++) {
        if (write(client_fd, data + i * chunk_size, chunk_size) == -1) {
            perror("write() error");
            exit(EXIT_FAILURE);
        }
    }

    // Send last chunk
    if (last_chunk_size > 0) {
        if (write(client_fd, data + num_chunks * chunk_size, last_chunk_size) == -1) {
            perror("write() error");
            exit(EXIT_FAILURE);
        }
    }
    
    if (!quiet_mode) printf("Data Sent amount: %lu\n", strlen(data));

    return client_fd;
}


