// EE122 Project 1 - Sender.c
// Xiaodian Wang    (SID: 20998240)
// Arnab Mukherji   (SID: )
/*
** Sender description: 
 *Given a connection type, the sender sets up the according socket and connects to the receiver. The sender will transmit data to the receiver by stream or by datagrams, depending on the specified connection type. 
 *Sender takes in three command-line arguments:
 argv[1]: A case-insensitive string of either 'DGRAM' or 'STREAM,' which will specify which socket type to use. Inputting 'DGRAM' will construct a datagram socket for a UDP connection. Inputting 'STREAM' will consruct a byte stream socket for a TCP connection.
 argv[2]: The numerical IPv4 address of the receiver that the sender wants to connect to.
 argv[3]: The name of the file that the sender will transmit to the receiver.
**Sender output: 
 *Sender returns the int 0 for a successful socket setup and transmission of data, and a nonzero integer otherwise.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//Define port that the Sender will be connecting to
#define PORT "65432"
//Define max amount of bytes we can send at once
#define MAXDATASIZE 1024

//Get the socket address, IPv6 or IPv6 (taken from Beej's guide)
/*If the sa_family field is AF_INET (IPv4), return the IPv4 address. Otherwise return the IPv6 address.*/
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* main() takes in 3 command-line arguments: argv[1] is a string of either 'DGRAM' or 'STREAM,' argv[2] is the destination IP address, and argv[3] is the file*/
int main(int argc, char *argv[]) {
    //Error check to make sure you're inputting the right # of args
    if (argc != 4) {
        perror("Error: incorrect number of command-line arguments\n");
        return 1;
    }
    
    //Declare variables for socket setup, used for both TCP & UDP connection
    const char *connection_option = argv[1];
    const char *dest_IP = argv[2];
    const char *target_file = argv[3];
    int sockfd;
    struct addrinfo hints, *receiver_info;
    int rv;
    char s[INET_ADDRSTRLEN];
    
    //Declare variables for file transfer
    int total_bytes_sent, bytes_left;
    unsigned char buffer[MAXDATASIZE];
    FILE *file_to_read;
    size_t bytes_sent;
    unsigned char *buffer_beg_ptr, *buffer_data_ptr;
    
    //Declare variables used in UDP file transfer only
    int packet_counter;
    uint32_t HEADER = 0xaaaaaaaa;
    uint32_t MAGICNUM = 0xdeadbeef;
    int last_packet;
    
    //Loading struct addrinfo hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    if (strcasecmp(connection_option, "stream") == 0) {
        hints.ai_socktype = SOCK_STREAM; //socket type will be stream
        printf("Establish a TCP connection\n");
    } else {
        hints.ai_socktype = SOCK_DGRAM; //socket type will be datagram
        printf("Establish a UDP connection\n");
    }
    
    //Get your target's (the receiver's) address information
    if ((rv = getaddrinfo(argv[2], PORT, &hints,
                          &receiver_info)) != 0) {
        perror("Unable to get receiver's address info\n");
        return 2;
    }
    
    //Take fields from first record in receiver_info, and create socket from it. (Beej's guide loops through all nodes to find an available one, I just take the first one)
    if ((sockfd = socket(receiver_info->ai_family,receiver_info->ai_socktype,receiver_info->ai_protocol)) == -1) {
        perror("Unable to create socket for the Receiver\n");
        return 3;
    }
    
    if (strcasecmp(connection_option, "stream") == 0) {
        //Connect to socket on the receiver's side. Connect() return 0 on success, and -1 on error. 
        if (connect(sockfd, receiver_info->ai_addr, receiver_info->ai_addrlen) == -1) {
            close(sockfd);
            perror("Sender: unable to connect\n");
            return 4;
        }
        //Error checking to see if receiver's info is null
        if (receiver_info == NULL) {
            fprintf(stderr, "Sender: no connection, no receiver info given\n");
            return 5;
        }
        
        //Print out the IP address of the receiver that the sender is connecting to
        inet_ntop(receiver_info->ai_family, get_in_addr((struct sockaddr*)receiver_info->ai_addr), s, sizeof s);
        printf("Sender: connecting to %s\n", s);
        
        //open the file data
        file_to_read = fopen(target_file, "r");
        if (file_to_read == NULL) {
            printf("Open file error: %s\n", target_file);
            return 6;
        }
        
    //Initialize the counter (total_bytes_sent) for the amount of bytes that the sender reads and sends to the receiver. Bytes_left and bytes_sent act to position where the data is read into the buffer. In the initial condition of the while loop, bytes_read = # of bytes read into the buffer, which may or may not be equal to the bytes_sent out of the buffer. bytes_sent is subtracted from bytes_left. bytes_sent is also added to the total number of bytes sent (total_bytes_sent), and the buffer pointer is shifed by the amount total_bytes_sent.
        total_bytes_sent = 0;
        while ((bytes_left = fread(buffer, 1, MAXDATASIZE, file_to_read)) != 0) {
            printf("Bytes read into buffer = %d\n", bytes_left);
            buffer_data_ptr = &buffer[0];
            while (bytes_left != 0) {
                bytes_sent = send(sockfd, buffer_data_ptr, (size_t)bytes_left, 0);
                printf("Bytes sent: %d\n", (int) bytes_sent);
                bytes_left -= (int) bytes_sent;
                total_bytes_sent += (int) bytes_sent;
                buffer_data_ptr += (int)bytes_sent;
            }
            printf("Total bytes sent = %d\n", total_bytes_sent);
        }
        
        /*Sending application knows that recipient has received all of the data if the last outputs of fread() and send() are >=0. If at any point the output of send() < 0, the while loop will terminate, which will return the last values of m and n anyways. TCP is a reliable transport protocol, so all bytes sent by the transmitter will be equal and same order as all bytes received by the recipient. */
        if (bytes_left < 0 || bytes_sent< 0) { //last outputs of send(), fread()
            printf("Error in file data reading/sending, receiver was not able to get all data\n");
        } else {
            printf("Receiver received all data, total bytes sent is %d\n", total_bytes_sent);
        }
        fclose(file_to_read);
        close(sockfd);
        return 0;
    }
    
    if (strcasecmp(connection_option, "dgram") == 0) {
        
        //Print out the IP address of the receiver that the sender is connecting to
        inet_ntop(receiver_info->ai_family, get_in_addr((struct sockaddr*)receiver_info->ai_addr), s, sizeof s);
        printf("Sender: sending data to %s\n", s);
        
        //open the file data
        file_to_read = fopen(target_file, "r");
        if (file_to_read == NULL) {
            printf("Open file error: %s\n", target_file);
            return 7;
        }
        
        /*Each packet that is sent out will have a 4-byte hex header, and a 1020-byte payload (the real file data). buffer_beg_ptr points to the beginning of the buffer where the hex header will go, and buffer_data_ptr points to the index where the rest of the payload will begin.*/
        total_bytes_sent = 0;
        int packet_counter = 1;
        buffer_beg_ptr = &buffer[0]; //pointer to position of 4-byte packet header
        buffer_data_ptr = &buffer[4]; //pointer to position of payload
        while ((bytes_left = fread (buffer_data_ptr, 1, MAXDATASIZE-4, file_to_read)) > 0) {
            printf("Number of bytes read: %d\n", bytes_left);
            uint32_t HEADER_netorder = htonl(HEADER);
            memcpy(&buffer[0], (void *)&HEADER_netorder, (size_t)sizeof HEADER);
        
            bytes_sent = sendto(sockfd, buffer_beg_ptr, bytes_left+4, 0, receiver_info->ai_addr, receiver_info->ai_addrlen);
            bytes_left -=(int)bytes_sent;
            total_bytes_sent += (int)bytes_sent;
            printf("Total bytes sent: %d\n", (int)total_bytes_sent);
            packet_counter++;
        }
        
        //Once we've reached the end (bytes_left = 0), we send out a last packet with the hex header oxDEADBEEF to let the receiver know that we are done.
        uint32_t MAGICNUM_netorder = htonl(MAGICNUM);
        memcpy(&buffer[0], (void *)&MAGICNUM_netorder, (size_t)sizeof MAGICNUM);
        last_packet = sendto(sockfd, buffer_beg_ptr, MAXDATASIZE, 0, receiver_info->ai_addr, receiver_info->ai_addrlen);
        fclose(file_to_read);
        close(sockfd);
        return 0;
    }
}