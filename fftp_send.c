#include "fftp.h"
#include <sys/time.h>

void datasegment(char* data, char* filename, unsigned int numport, size_t filesize){
    memcpy(data, filename, strlen(filename));
    memcpy(data + FILENAME_SIZE,(unsigned int*)&numport, PORTNUM_SIZE);
    memcpy(data + FILENAME_SIZE + PORTNUM_SIZE,(unsigned int*)&filesize,FILESIZE_SIZE);
}

int main(int argc, char* argv[]) {
    
    if (argc != 4) {
       fprintf(stderr,"ERROR: please provide valid arguments\nUsage: %s <hostname> <tcpport> <filename>\n", argv[0]);
       exit(0);
    }

    char mode;
    char* filename = argv[3];
    char buffer[SHK_PACKET_SIZE], nack_buffer[NACK_SIZE];
    int socktcp, sockfdudp;
    unsigned int seqleft, curseq, numseq, resend_port;
    unsigned int* seqptr;
    double totalseq, t_elps, rate;
    size_t filesize;
    pthread_t thread_id[PORT_NUM];
    struct hostent* server;
    struct timespec t_s, t_e;
    struct fftp_send info[PORT_NUM];
    struct sockaddr_in serv_addr_tcp;
    struct sockaddr_in serv_addr;

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(1);
    }

    socktcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socktcp < 0) 
        perror("ERROR opening socket");
    bzero((char*) &serv_addr_tcp, sizeof(serv_addr_tcp));
    serv_addr_tcp.sin_family = AF_INET;
    bcopy((char*) server->h_addr, (char*) &serv_addr_tcp.sin_addr.s_addr, server->h_length);
    serv_addr_tcp.sin_port = htons(atoi(argv[2]));


    if (access(filename, R_OK) != 0){ // File not exists
        perror("Input file not exists and cannot be read.");
        exit(-1);
    }

    FILE* f = fopen(filename, "r");
    fseek(f, 0, SEEK_END);
    filesize = ftell(f);


    bzero(buffer, SHK_PACKET_SIZE);
    datasegment(buffer, filename, PORT_NUM, filesize); // used for TCP communication

    if (connect(socktcp, (struct sockaddr *)&serv_addr_tcp,sizeof(serv_addr_tcp)) < 0){
        perror("ERROR connecting");
    }
    int n = (int)write(socktcp, buffer, sizeof(buffer)); //write the data to TCP packet
    if (n < 0) 
        perror("ERROR writing to socket");
    bzero(buffer, sizeof(buffer));
    // printf("tcp handshake completed.");

    // Initialize fftp_info for different threads
    totalseq = ceil((double)filesize / SIZE);
    seqleft = totalseq;
    curseq = 0;
    printf("Total number of seq is: %u\n", seqleft);
    for (size_t i = 0; i < PORT_NUM; i++) {
        info[i].filename = filename;
        info[i].numseq = (i == (PORT_NUM - 1)) ? seqleft + 1: (unsigned int)(totalseq / PORT_NUM);
        info[i].ffd = f;
        info[i].seqst = curseq;
        info[i].portno = 60000 + i;
        info[i].tid = thread_id[i];
        info[i].server = server;
        seqleft -= info[i].numseq;
        curseq += info[i].numseq;
        printf("Thread %u numbser of seq: %u; seq range: %u:%u\n", i, info[i].numseq, info[i].seqst, info[i].seqst+info[i].numseq-1);
    }
    clock_gettime(CLOCK_REALTIME, &t_s);
    for (size_t i = 0; i < PORT_NUM; i++) {
        pthread_create(&thread_id[i], NULL, fftp_send, &info[i]); 
    }
    
    // Open a socket and configure serv_addr parameters
    sockfdudp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfdudp < 0) 
        perror("ERROR opening socket\n");

    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*) server->h_addr, (char*) &serv_addr.sin_addr.s_addr, server->h_length);

    bzero(nack_buffer, NACK_SIZE);
    while(1) {
        n = read(socktcp, nack_buffer, NACK_SIZE);
        if (n < 0) {
            perror("ERROR reading from socket\n");
            exit(1);
        }
        memcpy((void*) &mode, nack_buffer, MODE_SIZE);
        if (mode == NACK) {
            memcpy((void*)&resend_port, nack_buffer+MODE_SIZE, PORT_LEN);
            memcpy((void*)&numseq, nack_buffer+MODE_SIZE+PORT_LEN, SEQ_LEN);
            if (numseq > 0){
                serv_addr.sin_port = htons(resend_port);
                seqptr = (unsigned int*)(nack_buffer + MODE_SIZE + PORT_LEN + SEQ_LEN);
                nack_reply(f, numseq, seqptr, SIZE, sockfdudp, &serv_addr);
            }

        } else if (mode == END) {
            memcpy(&t_e, nack_buffer+MODE_SIZE, sizeof(struct timespec));
            t_elps = difftime(t_e.tv_sec, t_s.tv_sec) + difftime(t_e.tv_nsec, t_s.tv_nsec)/1000000000;
            printf("Start time: %ld. End time: %ld. Time elapsed: %f seconds.\n", t_s.tv_sec, t_e.tv_sec, t_elps);
            rate = filesize / t_elps / 1000000 * 8;
            printf("The transfer rate is: %fMbps\n", rate);
            break;
        }
        bzero(nack_buffer, NACK_SIZE);
    }
    fclose(f);
    close(socktcp);
    return 0;
}
