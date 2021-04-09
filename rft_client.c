#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include "rft_util.h"
#include "rft_client_util.h"

/*
 * This file contains the main function for the client.
 * Do NOT change anything in this file.
 * 
 * For a usage message for the client type:
 * 
 *      rft_client
 *    
 * this will output a message explaining the command line options.
 *
 * Or start server as:
 *
 *      rft_client <input_file> <output_file> <server_addr> <port> 
 *                  <nm|wt loss_probability>  
 *
 * Where:
 *      input_file is the file to send
 *      output_file is name for the file on the server
 *      server_addr is the address of the server
 *      port is the port the server is listening on
 *      nm selects normal transfer mode
 *      wt selects transfer with time out and a probability of loss or 
 *          corruption of segments. The probability must be between 0.0 and 1.0,
 *          inclusive.
 *
 * Only specify one transfer mode. That is, either nm or wt with a loss 
 * probability.      
 */

/* transfer mode set from command line arguments */
typedef enum {
    UNKNOWN_TFR_MODE = 0,
    NM_TFR_MODE,            // normal transfer (argument: nm)
    WT_TFR_MODE             // transfer with timeout (argument: wt)
} tfr_mode;

#define TMODE_S_SIZE 3      // size of transfer mode command line arg
static char* tmode_s[] = { "un", "nm", "wt" };  // transfer mode args

/* helper function to process command line arguments */
static void process_argv(char* input_file, char* output_file, int port, 
    int argc, char** argv, tfr_mode* tmode, float* loss_prob, 
    char* inf_msg_buf);

/* helper function to end session, output success message and close resources */
static void exit_success(char* inf_msg_buf, off_t fsize, char* input_file,
    size_t bytes, int infd, int sockfd);
    
/* the main function and entry point for rft_client */
int main(int argc,char *argv[]) {
    if (argc < 6) {
        printf("usage: %s <input_file> <output_file> <server_addr> <port>"
            " <nm|wt loss_probability>\n", argv[0]);
        printf("       input_file is the file to send\n");
        printf("       output_file is name for the file on the server\n");
        printf("       server_addr is the address of the server\n");
        printf("       port is the port the server is listening on\n");
        printf("       nm selects normal transfer, or:\n");
        printf("       wt selects transfer with time out \n");
        printf("          and a probability of loss between 0.0 and 1.0\n");
        exit(EXIT_FAILURE);
    }

    char* input_file = argv[1];
    char* output_file = argv[2];
    char* server_addr = argv[3];
    int port = atoi(argv[4]);
    
    tfr_mode tmode = UNKNOWN_TFR_MODE;
    float loss_prob = 0.0;
    char inf_msg_buf[INF_MSG_SIZE];  // to construct info messages    
    
    process_argv(input_file, output_file, port, argc, argv, &tmode, &loss_prob,
        inf_msg_buf);

    srand((unsigned) time(NULL));    // seed PRNG for is_corrupted function
      
    /* try opening input file */
    int infd = open(input_file, O_RDONLY);
    
    if (infd < 0) {
        snprintf(inf_msg_buf, INF_MSG_SIZE, "Could not open input file %s",
            input_file);
        exit_cerr(__LINE__, inf_msg_buf);
    }
        
    struct stat sbuf;
    int r = fstat(infd, &sbuf);
    if (r < 0)
        exit_cerr(__LINE__, "Could not stat input file");
        
    off_t fsize = sbuf.st_size;
        
    print_sep();
    snprintf(inf_msg_buf, INF_MSG_SIZE, 
            "Opened file: %s, size: %ld bytes", input_file, (long) fsize);
    print_cmsg(inf_msg_buf);
    print_sep();
    print_sep();

    /* create a UDP socket and assign all server information */
    struct sockaddr_in server;
    int sockfd = create_udp_socket(&server, server_addr, port);
    
    if (sockfd == -1)  {
        close(infd);
        exit(EXIT_FAILURE);
    }
    
    print_cmsg("Prepared for transfer, sending meta data"); 
     
    /* Send meta data to the server */
    if (!send_metadata(sockfd, &server, fsize, output_file)) {
        close(infd);
        exit_cerr(__LINE__, "Sending meta data failed");
    }
    
    size_t bytes = 0;

    if (!fsize) 
        exit_success(inf_msg_buf, fsize, input_file, bytes, infd, sockfd);
     
    switch (tmode) {
        case NM_TFR_MODE:
            bytes = send_file_normal(sockfd, &server, infd, fsize);
            break;
        case WT_TFR_MODE:
            bytes = send_file_with_timeout(sockfd, &server, infd, fsize,
                        loss_prob);
            break;
        default: 
            errno = EINVAL;
            exit_cerr(__LINE__, "Unknown transfer mode");
    }
    
    exit_success(inf_msg_buf, fsize, input_file, bytes, infd, sockfd);
} 

static void exit_success(char* inf_msg_buf, off_t fsize, char* input_file, 
    size_t bytes, int infd, int sockfd) {
    if (!fsize) {
        snprintf(inf_msg_buf, INF_MSG_SIZE, 
            "Input file: %s is empty (0 bytes)", input_file); 
        print_cmsg(inf_msg_buf);
        print_cmsg("Transfer terminated after sending metadata");
    } else {
        print_cmsg("Transfer complete");
        snprintf(inf_msg_buf, INF_MSG_SIZE, 
            "%zu bytes sent for transfer of file: %s of size: %ld",
            bytes, input_file, (long) fsize);
        print_cmsg(inf_msg_buf);
    }
    
    print_sep();
    print_sep();
    
    /* Close the file */
    close(infd);
    
    /* Close the socket */
    close(sockfd);
    
    exit(EXIT_SUCCESS);
}

static void process_argv(char* input_file, char* output_file, int port, 
    int argc, char** argv, tfr_mode* tmode, float* loss_prob, 
    char* inf_msg_buf) {
    
    if (strnlen(input_file, FILE_NAME_SIZE) == FILE_NAME_SIZE) {
        errno = EINVAL;
        exit_cerr(__LINE__, "Input file name is longer than max length");
    }
    
    if (strnlen(output_file, FILE_NAME_SIZE) == FILE_NAME_SIZE) {
        errno = EINVAL;
        exit_cerr(__LINE__, "Input file name is longer than max length");
    }

    if (!strncmp(input_file, output_file, FILE_NAME_SIZE)) {
        errno = EINVAL;
        exit_cerr(__LINE__, "Input and output files have the same name");
    }
            
    if (port < PORT_MIN || port > PORT_MAX) {
        errno = EINVAL;
        exit_cerr(__LINE__, "Port is outside valid range");
    }
    
    if (!strncmp(argv[5], tmode_s[NM_TFR_MODE], TMODE_S_SIZE) && argc == 6) {
        *tmode = NM_TFR_MODE;
    }
    
    if (!strncmp(argv[5], tmode_s[WT_TFR_MODE], TMODE_S_SIZE) && argc == 7) {
        *tmode = WT_TFR_MODE;
        *loss_prob = atof(argv[6]);

        if (signbit(*loss_prob) || isgreater(*loss_prob, 1.0)) {
            errno = EINVAL;
            exit_cerr(__LINE__, "Loss probability is outside valid range");
        }
    }
    
    if (*tmode != NM_TFR_MODE && *tmode != WT_TFR_MODE) {
        errno = EINVAL;
        snprintf(inf_msg_buf, INF_MSG_SIZE, "Invalid transfer mode %s",
            argv[5]);
        exit_cerr(__LINE__, inf_msg_buf);
    }        
}

