#include "./fftp.h"
#include <fcntl.h>

unsigned int f_read(FILE* fp, unsigned int offset, unsigned int len, char* buffer) {
    unsigned int l;
    fseek(fp, offset, SEEK_SET);
    l = fread(buffer, sizeof(char), len, fp); 
    return l;
}

void f_write(FILE* fp, unsigned int offset, char* data, unsigned int len, pthread_mutex_t* lock) {
    pthread_mutex_lock(lock); 
    fseek(fp, offset, SEEK_SET);
    fwrite(data, sizeof(char), len, fp);
    pthread_mutex_unlock(lock); 
}

int udp_listen(int portno, unsigned int seqst, unsigned int numseq, size_t size, FILE* f, int tcpfd, pthread_mutex_t* lock) {
    int sockfd, n;
    unsigned int seqleft, repeat, nextnack;
    char buffer[size+MODE_SIZE+SEQ_LEN];
    char mode;
    bool record[numseq];
    struct sockaddr_in serv_addr;
    struct udppkt udppkt_rec;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
    }
    
    bzero((void*) &serv_addr, (size_t) sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
    }
    
    seqleft = numseq;
    repeat = 0;
    nextnack = numseq - numseq / NACK_RATE;
    bzero((void*) record, (size_t) sizeof(record)); //set all the entry in record to 'false'
    SetSocketBlockingEnabled(sockfd, false); // set UDP socket to non-blocking
    while (1) {
        n = recv(sockfd, (void*) buffer, size + MODE_SIZE + SEQ_LEN, 0);
        if (n < 0) { // no data to read in sockfd
            if (repeat > REPEAT_THD) {
                nack_req(tcpfd, record, portno, seqst, 0, numseq);
                repeat = 0;
            } else {
                repeat++;
                continue;
            }
        }
        bzero((void*) &udppkt_rec, (size_t) sizeof(struct udppkt));
        memcpy(&mode, buffer, MODE_SIZE);
        memcpy(&udppkt_rec.seq, buffer + MODE_SIZE, SEQ_LEN);
        udppkt_rec.len = n - MODE_SIZE - SEQ_LEN;
        udppkt_rec.buf = (char*)buffer + MODE_SIZE + SEQ_LEN;

        if (seqleft <= nextnack) {
            nack_req(tcpfd, record, portno, seqst, 0, numseq);
            nextnack = seqleft - seqleft / NACK_RATE;
            repeat = 0;
        }

        if (mode == DATA) {
            // printf("seq: %u, original buffer address: %p, buffer address: %p, len: %u\n", udppkt_rec.seq, buffer, udppkt_rec.buf, udppkt_rec.len);
            if (*udppkt_rec.buf < 48) { // this is to prevent corrupted data 
                printf("Receiving null data. The seq num is %u\n", udppkt_rec.seq);
                continue;
            }
            if(udppkt_rec.seq > seqst + numseq) {
                sleep(100);
                printf("something is wrong.\n");
                continue;
            }
            // printf("seq: %u\n %s\n", udppkt_rec.seq, udppkt_rec.buf);
            if (!record[udppkt_rec.seq-seqst]) {
                f_write(f, udppkt_rec.seq*size, udppkt_rec.buf, udppkt_rec.len, lock);
                record[udppkt_rec.seq-seqst] = true;
                seqleft--;
                
                if (seqleft == 0) {
                    return 0;
                } else if (seqleft % (numseq/NUM_REPORT) == 0) {
                    printf("%u of sequence yet to received on port%u.\n", seqleft, portno);
                }
            }
        }
    }
}

int sendf_udp_socket(int sockfd, FILE* f, unsigned int seqst, unsigned int numseq, size_t length, const struct sockaddr_in serv_addr) {
    int n;
    size_t len;
    char buffer[MODE_SIZE+SEQ_LEN+length];
    char mode = DATA;
    
    bzero(buffer, sizeof(MODE_SIZE+SEQ_LEN+length));
    for (size_t cnt = 0; cnt < RPET; cnt++) {
        for (size_t seqcnt = seqst; seqcnt < seqst + numseq; seqcnt++) {
            // printf("sending seq %u\n", seqcnt);
            memcpy(buffer, &mode, MODE_SIZE);
            memcpy(buffer+MODE_SIZE, (unsigned int*) &seqcnt, SEQ_LEN); // Write seq into buffer
            len = f_read(f, seqcnt*length, length, buffer+MODE_SIZE+SEQ_LEN);
            n = sendto(sockfd, buffer, (size_t) sizeof(char)*(len+MODE_SIZE+SEQ_LEN), 0,\
            (struct sockaddr*) &serv_addr, (socklen_t) sizeof(serv_addr));
            sleep(SEND_DELAY);
            if (n < 0) {
                perror("ERROR writing to socket");
                return -1;
            }
            bzero(buffer, sizeof(MODE_SIZE+SEQ_LEN+length));
        }
    }
    return 0;
}

void* fftp_listen(void* vargp) {
    int err;
    struct fftp_rec* info = (struct fftp_rec*) vargp;
    err = udp_listen(info->portno, info->seqst, info->numseq, (size_t) SIZE, info->f, info->tcpfd, info->lock);
    if (err) {
        fprintf(stderr, "ERROR: error occurs during listening on udp socket on port %d.\n", info->portno);
    }
    printf("port%u has finished\n", info->portno);
    return NULL;
}

void* fftp_send(void* vargp) {
    int sockfd, err;
    struct sockaddr_in serv_addr;
    struct fftp_send info = *(struct fftp_send*) vargp;
    
    // Open a socket and configure serv_addr parameters
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket\n");

    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char*) info.server->h_addr, (char*) &serv_addr.sin_addr.s_addr, info.server->h_length);
    serv_addr.sin_port = htons(info.portno);

    // Open file, read and send to receiver through socket until EOF.
    err = sendf_udp_socket(sockfd, info.ffd, info.seqst, info.numseq, (size_t) SIZE, serv_addr);
    if (err) {
        fprintf(stderr, "ERROR: fail to send file %s.", info.filename);
    }
    return NULL;
}

void tcpdecode(TCPtype* data, int newsockfd) {
	ssize_t err; 
	char buffer[SHK_PACKET_SIZE];

	bzero(buffer, SHK_PACKET_SIZE);
	bzero(data, sizeof(TCPtype));
	err = read(newsockfd, buffer, SHK_PACKET_SIZE);
	if (err < 0) {
		perror("ERROR reading from socket");
		exit(1);
	}
    memcpy(data->filename, buffer, FILENAME_SIZE);
    data->portnum = *(buffer+FILENAME_SIZE);
    data->filesize = *(unsigned int*)(buffer+ FILENAME_SIZE + PORTNUM_SIZE);
}

int nack_req(int tcpfd, bool* record, unsigned int portno, unsigned int seqst, size_t st, size_t end) {

    char* buffer;
    char mode;
    unsigned int* seqptr;
    unsigned int numseq, curseq;
    // struct timespec t_s, t_e;
    // double t_elps;

    buffer = (char*) malloc(NACK_SIZE);
    bzero(buffer, NACK_SIZE);
    numseq = 0;
    seqptr = (unsigned int*)(buffer + MODE_SIZE + PORT_LEN + SEQ_LEN);
    // clock_gettime(CLOCK_REALTIME, &t_s);
    for (size_t i = st; i < end; i++) {
        if (record[i] == false) {
            curseq = seqst + i;
            memcpy(seqptr++, &(curseq), SEQ_LEN);
            numseq++;
            if (numseq >= NACK_PER_PKT) {
                break;
            }
        }
    }
    // clock_gettime(CLOCK_REALTIME, &t_e);
    // Write header fields into buffer
    mode = NACK;
    memcpy(buffer, &mode, MODE_SIZE);
    memcpy(buffer+MODE_SIZE, &(portno), PORT_LEN);
    memcpy(buffer+MODE_SIZE+PORT_LEN, &(numseq), SEQ_LEN);
    if (write(tcpfd, buffer, NACK_SIZE) < 0) { //write the data to TCP packet
        perror("ERROR writing to socket");
        bzero(buffer, NACK_SIZE);
    }
    // t_elps = difftime(t_e.tv_sec, t_s.tv_sec) + difftime(t_e.tv_nsec, t_s.tv_nsec)/1000000000;
    // printf("Takes %f seconds.\n", t_elps);
    // sleep(3);  
    return 0;
}

int nack_reply(FILE* f, unsigned int numseq, unsigned int* seqptr, size_t pkt_length, int udpfd, struct sockaddr_in* serv_addr) {
    int err;
    unsigned int len;
    char mode;
    char buffer[pkt_length+MODE_SIZE+SEQ_LEN];
    bzero(buffer, pkt_length+MODE_SIZE+SEQ_LEN);

    mode = DATA;
    memcpy(buffer, &mode, MODE_SIZE); 
    for (size_t i = 0; i < numseq; i++) {
        // printf("seq %u\n", *seqptr);
        memcpy(buffer+MODE_SIZE, seqptr, SEQ_LEN); // Write seq number into buffer
        len = f_read(f, (*seqptr++)*pkt_length, pkt_length, buffer+MODE_SIZE+SEQ_LEN);

        for (size_t cnt = 0; cnt < RESEND_RPET; cnt++) {
            err = sendto(udpfd, buffer, (size_t) sizeof(char)*(len+MODE_SIZE+SEQ_LEN), 0,\
            (struct sockaddr*) serv_addr, (socklen_t) sizeof(struct sockaddr_in));
            sleep(SEND_DELAY);
        }
        if (err < 0) {
            perror("ERROR writing to socket");
            return -1;
        }
    }
    return 0;
}
/** Returns true on success, or false if there was an error */
bool SetSocketBlockingEnabled(int fd, bool blocking) {
   if (fd < 0) return false;

    #ifdef _WIN32
        unsigned long mode = blocking ? 0 : 1;
        return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
    #else
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) return false;
        flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
    #endif
}