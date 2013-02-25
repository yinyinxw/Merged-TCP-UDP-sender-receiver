// EE122 Project 1 - Receiver.c
// Xiaodian Wang    (SID: 20998240)
// Arnab Mukherji   (SID:)
/*
** Receiver description: 
 * Given a connection type, it sets up the according socket, and waits for a connection from a transmitter. The transmitter will send data to the Receiver, with the Receiver will read into an output file. 
** Receiver input:
 * Receiver takes in 1 command-line argument, a case-insensitive string of either "DGRAM" or "STREAM", which will specify which socket type to use. Inputting "DGRAM" will construct a datagram socket to set up a UDP connection. Inputting "STREAM" will construct a byte stream socket to set up a TCP connection.
** Receiver output: 
 * Receiver returns the int 0 for successful socket setup and transmission of data, and a nonzero integer otherwise. An output file made from the received data will be constructed and contained in the same directory that the Receiver is in. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

//Define port that the Receiver will be connecting to
#define PORT "65432"
//For TCP connection, define the amount of pending connections queue will hold
#define BACKLOG 10
//Define max amount of bytes we can get at once. This will determine the size of the buffer (or, for UDP, the size of each packet). 
#define MAXDATASIZE 1024


//Get the socket address, IPv6 or IPv6 (taken from Beej's guide)
/*If the sa_family field is AF_INET (IPv4), return the IPv4 address. Otherwise return the IPv6 address.*/
void *get_in_addr(struct sockaddr *sa) { 
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* main() takes in 1 case-insensitive command-line argument: 'stream' or 'dgram,' and constructs a stream socket, or datagram socket, respectively.*/
int main (int argc, char *argv[]) {
    //declare variables for socket setup, used for both TCP and UDP connection
    const char *connection_option;
    int sockfd; //socket file descriptor
    struct addrinfo hints, *receiver_info; //receiver_info_ptr;<-don't know if used
    struct sockaddr_storage their_addr; //connector's address information
    char s[INET_ADDRSTRLEN];
    int yes = 1; //Use this for setting socket reuse option
    int rv;
    
    socklen_t sin_size; //used for TCP
    int new_fd; //used for TCP
    
    //declare variables for file writing
    FILE *file_to_write; //file handle
    char file_name_w[64];
    int total_bytes_read = 0;
    size_t num_from_remote;
    unsigned char buffer[MAXDATASIZE];
    
    //Declare variables used only in the UDP connection
    uint32_t packet_header;
    uint32_t header_host_num;
    int ack_msg = 1;
    int ack_msg_byte; 
    connection_option = argv[1];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE; //use my IP
    
    //Declare variables for getting the arrival time of packets for the UDP connection
    struct timeval current_time;
    suseconds_t delta_time, cumulative_delta = 0;
    suseconds_t arrival_time_1, arrival_time_2 = 0;
    suseconds_t avg_arrival_time;
    int receive_count;
    
    //Error check to make sure you're inputting the right # of args
    if (argc != 2) {
        perror("Error: incorrect number of command-line arguments\n");
        return 1;
    }
    if (strcasecmp(connection_option, "stream") == 0) {
        hints.ai_socktype = SOCK_STREAM; //socket type will be stream
        printf("Establish a TCP connection\n");
    } else {
        hints.ai_socktype = SOCK_DGRAM; //socket type will be datagram
        printf("Establish a UDP connection\n");
    }
    
    //Get receiver's address information
    if ((rv = getaddrinfo(NULL, PORT, &hints,
                          &receiver_info)) != 0) {
        perror("Unable to get receiver's address info\n");
        return 1;
    }
    
    //Take fields from first record in receiver_info, and create socket from it. (Beej's guide loops through all nodes to find an available one, I just take the first one)
    if ((sockfd = socket(receiver_info->ai_family,receiver_info->ai_socktype,receiver_info->ai_protocol)) == -1) {
        perror("Unable to create socket for the Receiver\n");
        return 2; 
    }
    
    //Allow for reuse of port immediately, as soon as the service exits.
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("Error with setting socket option for port reuse\n");
        return 3; 
    }
    
    //Bind the socket to the port using the first node of the receiver_info linked list.
    if (bind(sockfd, receiver_info->ai_addr, receiver_info->ai_addrlen) == -1) {
        close(sockfd);
        perror("Receiver: failed to bind socket to port\n");
        return 4;
    }
    
    if (receiver_info == NULL) {
        perror("Receiver: failed to bind socket to port because receiver's struct addrinfo is null\n");
        return 5; 
    }
    
    //STREAM option: establishing a TCP connection
    if (strcasecmp(connection_option, "STREAM") == 0) {
        printf("receiver: waiting for connections...\n");
        
        //listen for new connections
        if (listen(sockfd, BACKLOG) == -1) {
            perror("Stream connecter receiver: listen error\n");
            return 6;
        }
        while (1) {
            sin_size = sizeof their_addr;
            //accept() incoming connection, return a new socket descriptor
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
            
            //Print out IP address of accepted connection
            inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), s, sizeof s);
            printf("Receiver: got TCP connection from %s\n", s);
            
            if (new_fd > 0) {
                sprintf(file_name_w, "stream_file_written");
                file_to_write = fopen(file_name_w, "a");
                total_bytes_read = 0;
                //Receive data into buffer, and write into the file. The total amount of bytes read (total_bytes_read) is equal to the amount of bytes written into the file.
                while((num_from_remote = recv(new_fd, buffer, MAXDATASIZE - 1, 0)) != 0) {
                    total_bytes_read += fwrite(buffer, 1, num_from_remote, file_to_write);
                }
                fflush(file_to_write);
                fclose(file_to_write);
                close(new_fd);
                printf("Total bytes read over TCP connection: %d\n", total_bytes_read);
            } else {
                perror("TCP connection: socket accept error\n");
            }
            close (new_fd);
            return 0;
        }
    }
    
    if (strcasecmp (connection_option, "dgram") == 0) {
        printf("Datagram socket receiver: waiting to receive data...");

        total_bytes_read = 0;
        int packet_counter = 1;
        sprintf(file_name_w, "datagram_file_written");
        file_to_write = fopen(file_name_w, "a");
        socklen_t addr_len = sizeof their_addr;
                
        while ((num_from_remote = recvfrom(sockfd, buffer, MAXDATASIZE-1, 0, (struct sockaddr *)&their_addr, &addr_len)) > 0) {
            
            /*Compute the time difference between packets received.
             Calculates delta_time (microsec), which is the arrival time difference between
             2 consecutive packets. All of the delta values are added up into
             cumulate_delta.
             */
            gettimeofday(&current_time, 0);
            arrival_time_1 = current_time.tv_usec;
            printf("Arrival_time_1= %d\n", (int)arrival_time_1);
            if (arrival_time_2 != 0) {
                delta_time = arrival_time_1 - arrival_time_2;
                cumulative_delta += delta_time;
                printf("delta time is %d\n cumulative delta is %d\n", (int)delta_time, (int)cumulative_delta);

            }
            receive_count++;
            arrival_time_2 = arrival_time_1;
            
            memcpy((void *)&packet_header, &buffer[0], 4);
            header_host_num = ntohl(packet_header);
            printf("Receiving %d bytes\n", (int)num_from_remote);
            
            if (header_host_num == 0xdeadbeef) {
                //header_host_num = 0xdeadbeef, reached end of file transfer
                fclose(file_to_write);
                printf("Reached end of file, total number of packets received: %d\n", total_bytes_read);
                /*Calculate the average delay difference between consecutive individual packets, and use this delay to measure the bandwidth of the bottleneck link later on.*/
                printf("Final receive count: %d\n", receive_count);
                avg_arrival_time =cumulative_delta / receive_count;
                printf("Average delay difference btwn individual packets: %d microseconds\n", (int)avg_arrival_time);
                close(sockfd);
                return 0;
            }
            else {
                total_bytes_read +=fwrite(buffer+4, 1, num_from_remote-4, file_to_write);
            }
        }
    }

}

