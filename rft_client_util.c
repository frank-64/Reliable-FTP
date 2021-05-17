#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "rft_util.h"
#include "rft_client_util.h"


/* 
 * UTILITY FUNCTIONS PROVIDED FOR YOU 
 * Do NOT edit the following functions:
 *      is_corrupted (local static function)
 *      pint_cmsg
 *      print_cerr
 *      exit_cerr
 */

/* 
 * is_corrupted - returns true with the given probability 
 * 
 * The result can be passed to the checksum function to "corrupt" a 
 * checksum with the given probability to simulate network errors in 
 * file transfer 
 */
static bool is_corrupted(float prob) {
    float r = (float) rand();
    float max = (float) RAND_MAX;
    
    return (r / max) <= prob;
}

/* print client information messge to stdout */
void print_cmsg(char* msg) {
    print_msg("CLIENT", msg);
}

/* print client error message to stderr */
void print_cerr(int line, char* msg) {
    print_err("CLIENT", line, msg);
}

/* exit with a client error */
void exit_cerr(int line, char* msg) {
    print_cerr(line, msg);
    exit(EXIT_FAILURE);
}


/*
 * FUNCTIONS THAT YOU HAVE TO IMPLEMENT
 */

/*
 * See documentation in rft_client_util.h
 * Hints:
 *  - Remember to print appropriate information/error messages for 
 *    success and failure to open the socket.
 *  - Look at server code.
 */
int create_udp_socket(struct sockaddr_in* server, char* server_addr, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd == -1){
        exit_cerr(__LINE__, "Failed to open socket");
    }
    print_cmsg("Socket created");

    //memset(server, 0, sizeof(&server));

    server->sin_family = AF_INET;
    server->sin_addr.s_addr = inet_addr(server_addr);
    server->sin_port = htons(port);

    return sockfd;
 } 
  
/*
 * See documentation in rft_client_util.h
 * Hints:
 *  - The metadata will have a copy of the output file name that the 
 *      server will use as the name of the file to write to
 *  - Remember to close any resource you are given if sending fails
 *  - Look at server code.
 */
bool send_metadata(int sockfd, struct sockaddr_in* server, off_t file_size,
    char* output_file) {

    metadata_t metadata;

    metadata.size = file_size;
    strcpy(metadata.name, output_file);

    ssize_t bytes = sendto(sockfd, &metadata, sizeof(metadata_t),0, (struct  sockaddr*) server, sizeof(struct sockaddr_in));

    if(bytes <0 || !bytes){
        return false;
    } else{
        print_cmsg("Metadata sent");
        print_sep();
        return true;
    }
}
/*
 * See documentation in rft_client_util.h
 * Hints:
 *  - Remember to output appropriate information messages for the user to 
 *      follow progress of the transfer
 *  - Remember in this function you can exit with an error as long as you 
 *      close open resources you are given.
 *  - Look at server code.
 */
size_t send_file_normal(int sockfd, struct sockaddr_in* server, int infd,
    size_t bytes_to_read) {

    char msg_buffer[INF_MSG_SIZE];
    segment_t sg;
    segment_t ack_sg;
    sg.sq = 0;
    int total_bytes =0;
    int segment_amount = bytes_to_read / (PAYLOAD_SIZE-1);
    if ((bytes_to_read % (PAYLOAD_SIZE-1))>0){
        segment_amount = segment_amount + 1;
    }

    for (int i = 0; i < segment_amount; ++i) {
        memset(sg.payload, 0, PAYLOAD_SIZE);
        sg.sq = i;
        sg.type = DATA_SEG;
        sg.payload_bytes = read(infd, sg.payload, PAYLOAD_SIZE-1);

        if (sg.payload_bytes < (PAYLOAD_SIZE)) {
            sg.last = true;
        } else {
            sg.last = false;
        }

        sg.checksum = checksum(sg.payload, false);
        ssize_t bytes = sendto(sockfd, &sg, sizeof(sg), 0, (struct sockaddr*) server, sizeof (struct sockaddr_in));

        if (bytes < 0){
            close(sockfd);
            exit_cerr(__LINE__, "Sending message failed");
        } else{
            snprintf(msg_buffer, INF_MSG_SIZE, "Segment %d is being sent, payload bytes: %zu, checksum: %d", sg.sq, sg.payload_bytes, sg.checksum);
            print_cmsg(msg_buffer);

            print_cmsg("Waiting on ack");
            socklen_t address_length = (socklen_t) sizeof(struct sockaddr_in);
            ssize_t bytes_recv;
            bytes_recv = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr*) server, &address_length);

            if (bytes_recv < 0){
                close(sockfd);
                exit_cerr(__LINE__, "Reading acknowledgement failed");
            } else if (!bytes_recv){
                close(sockfd);
                exit_cerr(__LINE__, "No acknowledgement received");
            } else{
                snprintf(msg_buffer, INF_MSG_SIZE, "ACK with sq: %d received", ack_sg.sq);
                print_cmsg(msg_buffer);
                print_sep();
            }

            total_bytes = total_bytes + sg.payload_bytes;
        }
    }
    return total_bytes;
}


/*
 * See documentation in rft_client_util.h
 * Hints:
 *  - Remember to output appropriate information messages for the user to 
 *      follow progress of the transfer
 *  - Remember in this function you can exit with an error as long as you 
 *      close open resources you are given.
 *  - Look at server code.
 */
size_t send_file_with_timeout(int sockfd, struct sockaddr_in* server, int infd,
    size_t bytes_to_read, float loss_prob) {

    struct timeval timeval;
    timeval.tv_sec = 2;
    timeval.tv_usec = 0;

    int set_socket = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeval, sizeof (struct timeval));

    if (set_socket < 0){
        exit_cerr(__LINE__, "Unable to set socket");
    }
    char msg_buffer[INF_MSG_SIZE];
    segment_t sg;
    segment_t ack_sg;
    sg.sq = 0;
    int total_bytes =0;
    int segment_amount = bytes_to_read / (PAYLOAD_SIZE-1);
    if ((bytes_to_read % (PAYLOAD_SIZE-1))>0){
        segment_amount = segment_amount + 1;
    }

    for (int i = 0; i < segment_amount; ++i) {
        memset(sg.payload, 0, PAYLOAD_SIZE);
        sg.sq = i;
        sg.type = DATA_SEG;
        sg.payload_bytes = read(infd, sg.payload, PAYLOAD_SIZE-1);

        if (sg.payload_bytes < (PAYLOAD_SIZE)) {
            sg.last = true;
        } else {
            sg.last = false;
        }
        bool corrupted = is_corrupted(loss_prob);
        if (corrupted){
            print_cmsg("Segment corrupted");
        }
        sg.checksum = checksum(sg.payload, corrupted);
        ssize_t bytes = sendto(sockfd, &sg, sizeof(sg), 0, (struct sockaddr*) server, sizeof (struct sockaddr_in));



        if (bytes < 0){
            close(sockfd);
            exit_cerr(__LINE__, "Sending message failed");
        } else {
            snprintf(msg_buffer, INF_MSG_SIZE, "Segment %d is being sent, payload bytes: %zu, checksum: %d", sg.sq,
                     sg.payload_bytes, sg.checksum);
            print_cmsg(msg_buffer);

            print_cmsg("Waiting on ack");
            socklen_t address_length = (socklen_t) sizeof(struct sockaddr_in);
            ssize_t bytes_recv;
            bytes_recv = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr *) server, &address_length);

            while (bytes_recv < 0) {
                corrupted = is_corrupted(loss_prob);
                sg.checksum = checksum(sg.payload, corrupted);
                bytes = sendto(sockfd, &sg, sizeof(sg), 0, (struct sockaddr *) server, sizeof(struct sockaddr_in));
                bytes_recv = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr *) server,
                                      &address_length);
            }
            if (!bytes_recv) {
                close(sockfd);
                exit_cerr(__LINE__, "No acknowledgement received");
            } else {
                snprintf(msg_buffer, INF_MSG_SIZE, "ACK with sq: %d received", ack_sg.sq);
                print_cmsg(msg_buffer);
                print_sep();
                total_bytes = total_bytes + sg.payload_bytes;
            }
        }
    }
    return total_bytes;
}



