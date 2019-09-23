# EE542 Internet and Cloud Computing @ USC 

This repo contains all the source code, Makefile and README related to 2019 Fall EE542 lab3, Fast and Reliable File Transfer, at USC.

## Initial TCP handshake:
	1. Filename
	2. Number of UDP ports
	3. Size of file

## After TCP handshake:
	1. Open UDP sockets to send/receive data
	2. keep listening on TCP socket to look for NACK packet
	3. If receive NACK packets, use the UDP socket open by main thread to resend all missing packets
	
## Complation:
	To compile the code, simply clone this repo to your local machine, cd to the directory and type `make`. You should be able to see the executable files for both fftp_send and fftp_rec. Note that before all the source code got compiled, Make first clean up all the target executibles.

	If you want to clean up the old executables, simply type `make clean` to remove the old files.


## Milestone:

	1. File I/O (r/w certain location in file) and make sure the file types of files after r/w are the same. This is done in fftp.c f_write and f_read.

	2. Segmentation(sender) & desegmentation(receiver). This is done in udp_listen and sendf_udp_socket in fftp.h/.c

	3. timing. This is done with the lib <sys/time.h> and <time.h>. First, get the timestamp before start sending UDP packet. After last segment is received, receiver will notify host that all the segments are received and get another timestamp. The time elasped is calculated by substrating start time from end time.
	
	4. Loss handling: packet loss is handled by sending NACK from receiver with periods that gradually become shorter when the number of sequences yet to receive decrease. In addition, to prevent deadlock in the situation that sender sends out all the sequences but some of them are lost during transmission and receiver keeps waiting for the data without any action, nack in this case.

	5. Multi-threading: This is done with pthread library. User can control number of threads to transfer file with the macro 'PORT_NUM' in fftp.h.


Note:
	
[Random file accessing]

[Multithreading in C]

[Random file accessing]: https://www.thoughtco.com/random-access-file-handling-958450

[Multithreading in C]: https://dzone.com/articles/parallel-tcpip-socket-server-with-multi-threading