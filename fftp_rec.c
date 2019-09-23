#include "./fftp.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
       fprintf(stderr, "ERROR: please provide valid arguments\nUsage: %s <tcp_port>\n", argv[0]);
       exit(0);
    }

    int tcpport, socktcp, newsocktcp, pid;
    unsigned int seqleft, curseq;
    double totalseq;
    struct sockaddr_in serv_addrtcp, cli_addr;
    size_t clilen = sizeof(cli_addr);
    pthread_t* thread_id;
    pthread_mutex_t lock; 
    struct fftp_rec* info;
    FILE* f;
    TCPtype data; //define a TCPData struct data type


    //TCP part
    tcpport = atoi(argv[1]);
    socktcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socktcp < 0) {
        perror("ERROR opening socket");
    } 
    bzero((void*) &serv_addrtcp, (size_t) sizeof(serv_addrtcp));
    serv_addrtcp.sin_family = AF_INET;
    serv_addrtcp.sin_addr.s_addr = INADDR_ANY;
    serv_addrtcp.sin_port = htons(tcpport);
    if (bind(socktcp, (struct sockaddr*) &serv_addrtcp, sizeof(serv_addrtcp)) < 0) {
        perror("ERROR on binding");
    }
    
    if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("mutex init has failed\n"); 
        return -1; 
    }

    listen(socktcp, 5); //for TCP

    while (1) {
        newsocktcp = accept(socktcp, (struct sockaddr *) &cli_addr, &clilen);
        if (newsocktcp < 0) {
            printf("ERROR on accept");
            exit(1);
        }
        pid = fork(); 
        if (pid < 0) {
            perror("ERROR on fork");
            exit(1);
        }
        if (pid == 0) { 
            close(socktcp);

            //Get TCP control information
            tcpdecode(&data, newsocktcp);
            printf("file name:%s\n", data.filename);
            printf("port num:%u\n", data.portnum);
            printf("file size:%u\n", data.filesize);
            
            thread_id = (pthread_t*) malloc(sizeof(pthread_t) * data.portnum);
            info = (struct fftp_rec*) malloc(sizeof(struct fftp_rec) * data.portnum);
            f = fopen(data.filename, "w");
            if (f == NULL) {
                fprintf(stderr, "ERROR: error occurs during file opening. Exit.\n");
                exit(0);
            }
            totalseq = ceil((double)data.filesize / SIZE);
            seqleft = totalseq;
            curseq = 0;
            // printf("Total number of seq to receive: %lf.\n", totalseq);
            for (size_t i = 0; i < data.portnum; i++) {
                // printf("Creating thread%u\n", i);
                info[i].f = f;
                info[i].tcpfd = newsocktcp;
                info[i].numseq = (i == (PORT_NUM - 1)) ? seqleft + 1: (unsigned int)(totalseq / PORT_NUM);
                info[i].seqst = curseq;
                info[i].portno = 60000 + i;
                info[i].tid = thread_id[i];
                info[i].lock = &lock;
                seqleft -= info[i].numseq;
                curseq += info[i].numseq;
                // printf("Thread%lu, numseq: %u, seqst:%u, portno:%u\n", i, info[i].numseq, info[i].seqst, info[i].portno);
                pthread_create(&thread_id[i], NULL, fftp_listen, &info[i]); 
            }

            // Wait for all the thread to complete
            for (size_t i = 0; i < data.portnum; i++) {
                pthread_join(thread_id[i], NULL);
            }
            printf("All threads have finished.\n");
            fftp_end(newsocktcp);
            fclose(f);
            exit(0);
        }
        else {
            close(newsocktcp);
        }
    }
}
void fftp_end(int tcpfd) {
    char buffer[SHK_PACKET_SIZE];
    buffer[0] = END;
    struct timespec t_e;
    clock_gettime(CLOCK_REALTIME, &t_e);
    // gettimeofday(&t_e, NULL);
    memcpy(buffer+MODE_SIZE, &t_e, sizeof(struct timespec));
    if (write(tcpfd, buffer, MODE_SIZE + sizeof(struct timespec)) < 0) { //write the data to TCP packet
        perror("ERROR writing to socket");
    }  
    return;
}