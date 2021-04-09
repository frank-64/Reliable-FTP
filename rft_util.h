#ifndef _RFT_H
#define _RFT_H
#include <stdbool.h>
#include <unistd.h>

#define FILE_NAME_SIZE 56   // max size of a file name (length if 55)
#define PAYLOAD_SIZE 36     // max size of file content payload to send in 
                            // each segment (36 bytes, length of string: 35)
#define INF_MSG_SIZE 256    // max size of information messages to print out
#define PORT_MIN 1025       // minimum network port number to use
#define PORT_MAX 65535      // maximum network port number to use

/* metadata to send to prepare for a file transfer */
typedef struct metadata {
    off_t size;                 // size of the file to send
    char name[FILE_NAME_SIZE];  // name of the file to create on server
} metadata_t;

/* segment types */
typedef enum {
  DATA_SEG,    // data segment
  ACK_SEG      // ack segment
} seg_type;

/* segment definition for chunks of file transfer data */
typedef struct segment {
    int sq;                         // sequence number of segment
    seg_type type;                  // segment type
    bool last;                      // last segment flag
    int checksum;                   // checksum of payload
    size_t payload_bytes;           // bytes of payload (not incl. '\0')
    char payload[PAYLOAD_SIZE];     // payload data (file content in chunks)
} segment_t;


/*
 * checksum - calculates a checksum from a segment's payload data
 *
 * Parameters:
 * payload - a pointer to the payload
 * is_corrupted - a flag to indicate whether the checksum should be corrupted
 *      to simulate a network error (set for true in some cases for Part 2 of
 *      the assignment)
 *
 * Return:
 * An integer value calculated from the payload of a segment
 */
int checksum(char *payload, bool is_corrupted);

/* 
 * Information message functions 
 */
void print_sep();                       // print a separator to demarcate output
void print_msg(char* role, char* msg);  // print given information message to
                                        // to stdout, for client or server role
void print_err(char* role, int line, char* msg); 
                                        // print message to stderr for error
                                        // detected at given line, for 
                                        // specified client or server role
#endif