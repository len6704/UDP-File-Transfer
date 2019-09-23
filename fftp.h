#ifndef FFTP_H
#define FFTP_H 

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

// Max IP packet size minus UDP header and fftp header size
#define SIZE 65000
// Number of copy to send for normal seq 
#define RPET 2
// Number of copy to send for resend seq 
#define RESEND_RPET 2
// Number of UDP port to use to send pkt in parallel
#define PORT_NUM 10
// The size of the 'mode' field in TCP control pkt
#define MODE_SIZE 1
// The size of the 'filename' field in TCP control pkt during initial handshake
#define FILENAME_SIZE 32
// The size of the 'portnum' field in TCP control pkt during initial handshake
#define PORTNUM_SIZE 4
// The size of the 'filesize' field, indicating the size of the file will be sending in TCP control pkt during initial handshake
#define FILESIZE_SIZE 8
// The length of port number in header
#define PORT_LEN 4
// The length of sequence number in header
#define SEQ_LEN 4
// The total size in TCP control pkt during initial handshake
#define SHK_PACKET_SIZE MODE_SIZE + FILENAME_SIZE + PORTNUM_SIZE + FILESIZE_SIZE
// The size of NACK packet
#define NACK_SIZE 1024
// Number of seq number each NACK packet can contain
#define NACK_PER_PKT (NACK_SIZE - MODE_SIZE - PORTNUM_SIZE * 2) / SEQ_LEN
// The delay between sending concecutive pkt to prevent congestion in network
// #define SEND_DELAY 1 / 10000 * PORT_NUM
#define SEND_DELAY 0
// The constant to control the frequency to send nack; The higher the rate is the more frequent the nack will be sent
#define NACK_RATE 10 
// Number of empty read from buffer before send out NACK 
#define REPEAT_THD 1000000
// Number of report telling how many seq left to receiver during transfer
#define NUM_REPORT 3 
// The identifers used by 'mode' in header
#define HDSK 1
#define NACK 2
#define ACK 3
#define DATA 4
#define END 5

struct udppkt
{
// use the size of an unisgned int as the len of seq number in udp header 
    unsigned int seq, len;
    char* buf;
};

struct fftp_send
{
    int portno;
    char* filename;
    FILE* ffd;
    unsigned int numseq; // Number of seq this thread should receive
    size_t seqst; // The first sequence number this thread should receive
    pthread_t tid; // The thread id for this thread
    struct hostent* server;
};

struct fftp_rec {
    int portno; // Port number of listen for the data
    int tcpfd; // The file handler of tcp socket to send NACK information
    unsigned int numseq; // Number of seq this thread should receive
    size_t seqst; // The first sequence number this thread should receive
    pthread_t tid; // The thread id for this thread
    FILE* f; // The file descripter for the file to write received data into
    pthread_mutex_t* lock; // The lock to write to file
};

struct TCPData {
    // 1 -> handshake; 2 -> NACK
    // char mode;  
    char filename[FILENAME_SIZE];
    unsigned int portnum;
    size_t filesize; 
};

typedef struct TCPData TCPtype;

/* Read 'len' byte from a file handler 'fp' from 'offset' position. Return NULL if error.*/
unsigned int f_read(FILE* fp, unsigned int offset, unsigned int len, char* buffer);
/* Write 'len' byte of 'data' to a file handler 'fp' from 'offset' position. Return non-0 if fails.*/
void f_write(FILE* fp, unsigned int offset, char* data, unsigned int len, pthread_mutex_t* lock);

/* Create a UDP socket listening on given port on localhost*/
int udp_listen(int portno, unsigned int seqst, unsigned int numseq, size_t size, FILE* f, int tcpfd, pthread_mutex_t* lock);

int sendf_udp_socket(int sockfd, FILE* f, unsigned int seqst, unsigned int numseq, size_t length, const struct sockaddr_in serv_addr);

void* fftp_listen(void* vargp);

void* fftp_send(void* vargp);

void tcpdecode(TCPtype* data, int newsockfd);

int nack_req(int tcpfd, bool* record, unsigned int portno, unsigned int seqst, size_t st, size_t end);

int nack_reply(FILE* f, unsigned int numseq, unsigned int* seqptr, size_t pkt_length,int udpfd, struct sockaddr_in* serv_addr);

void fftp_end(int tcpfd);

bool SetSocketBlockingEnabled(int fd, bool blocking);

#endif // FFTP_H 

