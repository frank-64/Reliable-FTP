#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> // for sockaddr_in
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "rft_util.h"

/*
 * This file contains the main function for the server.
 * Do NOT change anything in this file.
 * 
 * For a usage message for the server type:
 * 
 *      rft_server
 *    
 * this will output a message explaining the command line options.
 *
 * Or start server as:
 *      
 *      rft_server <port>
 *
 * where port is a port for the server to listen on in the range 1025 to 65535
 */

/* 
 * receive_file - receive a file on the given socket from the given client 
 * with given file metadata (expected size and name to write output to)
 */
static void receive_file(int sockfd, struct sockaddr_in* client, 
    metadata_t* file_inf);

/* 
 * process_data_msg - function used by receive_file to process a single data
 * segment and send ack to client and write payload to file
 * returns indication of whether still in receiving state (or last segment
 * has been received).
 */
static bool process_data_msg(int sockfd, struct sockaddr_in* client, 
    bool* first_seg, segment_t* data_msg, FILE* out_file);

/* 
 * Functions for information and error messages.
 */
static void print_smsg(char* msg);          // print server information message
static void print_serr(int line, char* msg);    // print server error message
static void exit_serr(int line, char* msg); // exit server with error message

/* the main function and entry point for rft_server */
int main(int argc,char *argv[]) {
    /* user needs to enter the port number */
    if (argc < 2) {
        printf("usage: %s <port>\n", argv[0]);
        printf("       port is a number between 1025 and 65535\n");
        exit(EXIT_FAILURE);
    }
    
    int port = atoi(argv[1]);
    
    if (port < PORT_MIN || port > PORT_MAX) 
        exit_serr(__LINE__, "Port is outside valid range");
    
    /* create a socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sockfd == -1)
        exit_serr(__LINE__, "Failed to open socket"); 
        
    print_sep();
    print_smsg("Socket created");
 
    /* set up address structures */
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t sock_len = (socklen_t) sizeof(struct sockaddr_in); 
    memset(&server, 0, sock_len);
    memset(&client, 0, sock_len);
    
    /* Fill in the server address structure */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);  // convert to network byte order
    
    /* bind/associate the socket with the server address */
    if(bind(sockfd, (struct sockaddr *) &server, sock_len)) {
        close(sockfd);
        exit_serr(__LINE__, "Bind failed");
    }
    
    print_smsg("Bind success ... "
                        "Ready to receive meta data from client"); 
    print_sep();
    print_sep();
      
    metadata_t file_inf;
    socklen_t addr_len = sock_len;
    ssize_t bytes = -1;

    //receive from the client    
    bytes = recvfrom(sockfd, &file_inf, sizeof(metadata_t), 0, 
                    (struct sockaddr*) &client, &addr_len);
    if (bytes < 0 ) {
        close(sockfd);
        exit_serr(__LINE__, "Reading stream message error");
    } else if (!bytes) {
        errno = ENOMSG;
        exit_serr(__LINE__, "Ending connection - no metadata received");
    } else {
        print_smsg("Meta data received successfully");
        char inf_msg_buf[INF_MSG_SIZE];              
        snprintf(inf_msg_buf, INF_MSG_SIZE, 
            "Output file name: %s, expected file size: %ld", file_inf.name, 
            (long) file_inf.size);
        print_smsg(inf_msg_buf);
    }
    
    print_sep();
    print_sep();
    print_smsg("Waiting for the file ..."); 
    
    receive_file(sockfd, &client, &file_inf);
    
    close(sockfd);
    
    struct stat stat_buf;
    stat(file_inf.name, &stat_buf);
    char inf_msg_buf[INF_MSG_SIZE];
    snprintf(inf_msg_buf, INF_MSG_SIZE, "%ld bytes written to file %s",
        (long) stat_buf.st_size, file_inf.name);
        
    print_smsg(inf_msg_buf);
    print_sep();
    print_sep();
    

    return EXIT_SUCCESS;
}

static void receive_file(int sockfd, struct sockaddr_in* client, 
    metadata_t* file_inf) {
    socklen_t addr_len = (socklen_t) sizeof(struct sockaddr_in);
    segment_t data_msg;
        
      
    /* Open the output file */
    FILE* out_file = fopen(file_inf->name, "w");
    
    if (!out_file) 
        exit_serr(__LINE__, "Could not open output file");
        
    // don't wait for empty file
    if (!file_inf->size) {
        fclose(out_file);
        return;
    }
                
    print_sep();
    print_sep();
    
    bool receiving = true;
    bool first_seg = true;
    size_t seg_size = sizeof(segment_t);

    /* while still receiving segments */
    while (receiving) {
        memset(&data_msg, 0, seg_size);
        ssize_t bytes = recvfrom(sockfd, &data_msg, seg_size, 0,
                        (struct sockaddr*) client, &addr_len);
        
        if (bytes < 0) {
            fclose(out_file);
            exit_serr(__LINE__, "Reading stream message error");
        } else if (!bytes) {
            print_smsg("Ending connection");
            receiving = false;
        } else {
            receiving = process_data_msg(sockfd, client, &first_seg, &data_msg,                 
                            out_file);
        }
    }
    
    print_smsg("File copying complete");
    
    print_sep();
    
    fclose(out_file);
}

static bool process_data_msg(int sockfd, struct sockaddr_in* client, 
    bool* first_seg, segment_t* data_msg, FILE* out_file) {
    bool receiving = true;
    char inf_msg_buf[INF_MSG_SIZE];
    size_t seg_size = sizeof(segment_t);

    if (*first_seg) {
        /* first segment to be received */
        print_smsg("File transfer started"); 
        print_sep();
    }

    snprintf(inf_msg_buf, INF_MSG_SIZE, 
        "Received segment with sq: %d, payload bytes: %zu, checksum: %d", 
        data_msg->sq, data_msg->payload_bytes, data_msg->checksum);
    print_smsg(inf_msg_buf);
    
    if (data_msg->payload[PAYLOAD_SIZE - 1]) {
        print_smsg("Payload not terminated");
        print_smsg("Did NOT send any ACK");
        print_sep();
        return receiving;
    }

    snprintf(inf_msg_buf, INF_MSG_SIZE, "Received payload:\n%s",
        data_msg->payload);
    print_smsg(inf_msg_buf);
    print_sep();

    int cs = checksum(data_msg->payload, false);

    /* 
     * If the calculated checksum is same as that of recieved 
     * checksum then send corrosponding ack
     */
    if (cs == data_msg->checksum) {
        /* is it the last segment or will we still be receiving*/
        receiving = !data_msg->last;
         
        snprintf(inf_msg_buf, INF_MSG_SIZE, "Calculated checksum %d VALID",
            cs);
        print_smsg(inf_msg_buf);
    
        /* Prepare the Ack segment */
        segment_t ack_msg;
        memset(&ack_msg, 0, seg_size);
        ack_msg.sq = data_msg->sq;
        ack_msg.type= ACK_SEG;
    
        snprintf(inf_msg_buf, INF_MSG_SIZE, "Sending ACK with sq: %d",
            ack_msg.sq);
        print_smsg(inf_msg_buf);
    
        /* Send the Ack segment */
        ssize_t bytes = sendto(sockfd, &ack_msg, seg_size, 0,
                    (struct sockaddr*) client, sizeof(struct sockaddr_in));
                    
        if (bytes < 0) {
            print_serr(__LINE__, "Sending stream message error");
        } else if (!bytes) {
            print_smsg("Ending connection");
        } else {
            /* write the payload of the data segment to output file */
            fprintf(out_file, "%s", data_msg->payload);
            printf("        >>>> NETWORK: ACK sent successfully <<<<\n");
            *first_seg = false;
        }
     
        print_sep();
        print_sep();
    } else {
        snprintf(inf_msg_buf, INF_MSG_SIZE, "Segment checksum %d INVALID",
            cs);
        print_smsg(inf_msg_buf);
        print_smsg("Did NOT send any ACK");
        print_sep();
    }    
    
    return receiving;
}

static void print_smsg(char* msg) {
    print_msg("SERVER", msg);
}

static void print_serr(int line, char* msg) {
    print_err("SERVER", line, msg);
}

static void exit_serr(int line, char* msg) {
    print_serr(line, msg);
    exit(EXIT_FAILURE);
}




 