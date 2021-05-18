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
        close(sockfd);
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

    segment_t data_sg;
    segment_t ack_sg;
    data_sg.sq = 0;
    int total_bytes =0;

    int segment_amount = bytes_to_read / (PAYLOAD_SIZE-1);
    if ((bytes_to_read % (PAYLOAD_SIZE-1))>0){
        segment_amount = segment_amount + 1;
    }

    for (int i = 0; i < segment_amount; ++i) {
        memset(data_sg.payload, 0, PAYLOAD_SIZE);
        data_sg.sq = i;
        data_sg.type = DATA_SEG;
        data_sg.payload_bytes = read(infd, data_sg.payload, PAYLOAD_SIZE-1);

        if (i == segment_amount-1) {
            data_sg.last = true;
        } else {
            data_sg.last = false;
        }

        data_sg.checksum = checksum(data_sg.payload, false);
        ssize_t bytes = sendto(sockfd, &data_sg, sizeof(data_sg), 0, (struct sockaddr*) server, sizeof (struct sockaddr_in));

        if (bytes < 0){
            close(sockfd);
            exit_cerr(__LINE__, "Sending message failed");
        } else{
            snprintf(msg_buffer, INF_MSG_SIZE, "Segment with sq: %d sent, payload bytes: %zu, checksum: %d", data_sg.sq, data_sg.payload_bytes, data_sg.checksum);
            print_cmsg(msg_buffer);

            snprintf(msg_buffer, INF_MSG_SIZE, "Sent payload:\n%s", data_sg.payload);
            print_cmsg(msg_buffer);
            print_sep();

            print_cmsg("Waiting for an ACK");
            socklen_t address_length = (socklen_t) sizeof(struct sockaddr_in);
            ssize_t bytes_received;
            ack_sg.type = ACK_SEG;
            bytes_received = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr*) server, &address_length);

            if (bytes_received < 0){
                close(sockfd);
                exit_cerr(__LINE__, "Reading ACK failed");
            } else if (!bytes_received){
                close(sockfd);
                exit_cerr(__LINE__, "No ACK received. Connection ending.");
            } else{
                snprintf(msg_buffer, INF_MSG_SIZE, "ACK with sq: %d received", ack_sg.sq);
                print_cmsg(msg_buffer);
                print_sep();
            }

            total_bytes = total_bytes + data_sg.payload_bytes;
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
    segment_t data_sg;
    segment_t ack_sg;
    data_sg.sq = 0;
    int total_bytes =0;
    int segment_amount = bytes_to_read / (PAYLOAD_SIZE-1);
    if ((bytes_to_read % (PAYLOAD_SIZE-1))>0){
        segment_amount = segment_amount + 1;
    }

    for (int i = 0; i < segment_amount; ++i) {
        memset(data_sg.payload, 0, PAYLOAD_SIZE);
        data_sg.sq = i;
        data_sg.type = DATA_SEG;
        data_sg.payload_bytes = read(infd, data_sg.payload, PAYLOAD_SIZE-1);

        if (i == segment_amount-1) {
            data_sg.last = true;
        } else {
            data_sg.last = false;
        }
        bool corrupted = is_corrupted(loss_prob);
        data_sg.checksum = checksum(data_sg.payload, corrupted);
        ssize_t bytes = sendto(sockfd, &data_sg, sizeof(data_sg), 0, (struct sockaddr*) server, sizeof (struct sockaddr_in));

        if (bytes < 0){
            close(sockfd);
            exit_cerr(__LINE__, "Sending message failed");
        } else {
            snprintf(msg_buffer, INF_MSG_SIZE, "Sent payload:\n%s", data_sg.payload);
            print_cmsg(msg_buffer);
            print_sep();

            print_cmsg("Waiting for an ACK");
            socklen_t address_length = (socklen_t) sizeof(struct sockaddr_in);
            ssize_t bytes_recv;
            ack_sg.type = ACK_SEG;
            bytes_recv = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr *) server, &address_length);

            while (bytes_recv < 0) {
                corrupted = is_corrupted(loss_prob);
                if (corrupted){
                    print_cmsg("Segment was corrupted and timed out. Resending...");
                }
                data_sg.checksum = checksum(data_sg.payload, corrupted);
                bytes = sendto(sockfd, &data_sg, sizeof(data_sg), 0, (struct sockaddr *) server, sizeof(struct sockaddr_in));
                bytes_recv = recvfrom(sockfd, &ack_sg, sizeof(segment_t), 0, (struct sockaddr *) server,
                                      &address_length);
            }
            if (!bytes_recv) {
                close(sockfd);
                exit_cerr(__LINE__, "No ACK received. Connection ended.");
            } else {
                snprintf(msg_buffer, INF_MSG_SIZE, "ACK with sq: %d received", ack_sg.sq);
                print_cmsg(msg_buffer);
                print_sep();
                total_bytes = total_bytes + data_sg.payload_bytes;
            }
        }
    }
    return total_bytes;
}
