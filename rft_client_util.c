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
    /* Replace the following with your function implementation */
    errno = ENOSYS;
    exit_cerr(__LINE__, "send_file_with_timeout is not implemented");

    return 0;
}



