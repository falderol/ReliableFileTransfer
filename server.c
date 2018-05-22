////////////////////////////////////////////////////////////////////////////////
// SERVER
// NOTES
// Maximum size of a packet we send is 512 bytes
// HEADER FORMAT
// Sequence Number 4 Bytes (0 is reservered for handshake, packet number starts
// at 1)
// Flags 1 Byte (Teardown)
// HANDSHAKE
// First thing recieved will unclude the file we want to send, send and wait up
// to 2 seconds for a reply, if no reply resend up to 8 times, if requested file
// is invalid respond with appropriate error
// RESPONSES
// We are going to start with ping pong. We will wait until we recieve a message
// from the client, and then send the segment that is one larger than the
// sequence number in it, keeping track of what recieved acknowledgements we
// have recieved to avoid resending stuff that we know the client has
// TEARDOWN
// Set the teardown flag for the last packet we send set the teardown flag
// If we recieve a response with the teardown flag, teardown, else wait 2
// seconds and resend up to 8 times, then teardown.
// TODO
// Parse requested file into 507 byte chunks
// create and insert header
// send
// wait some time, if response reply with requested chunk or error,
// if no response in that time, resend
// teardown
// TODO Eventually
//
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFSIZE 512
#define BUF_DATA 506
#define BUF_HEADER 5

void error(char *msg) {
    perror(msg);
    // exit(1);
}
/*
long file_size(char *name) {
    FILE *fp = fopen(name, "r");
    long size = -1;
    if (fp == NULL){
        
    }
    else if (fp) {
        //fseek (fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
    }
    return size;
}
*/
int main(int argc, char **argv) {
    int sockfd; // socket
    int portno; // port to listen on
    int clientlen; // byte size of client's address
    struct sockaddr_in serveraddr; // server's addr
    struct sockaddr_in clientaddr; // client addr
    struct hostent *hostp; // client host info
    char buf[BUFSIZE]; // message buf
    char *hostaddrp; // dotted decimal host addr string
    int optval; // flag value for setsockopt
    int n; // message byte size

    /////
    // Get Port from argument
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    // Create Socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /////
    // Eliminates "ERROR on binding: Address already in use" error.
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *) &optval, sizeof(int));
    /////
    // Set timout
     struct timeval tv;
     tv.tv_sec = 2; // Timeout in seconds
     setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
     					 (struct timeval *)&tv, sizeof(struct timeval));
    /////
    // Build servers address
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr("10.121.217.7");
    serveraddr.sin_port = htons((unsigned short) portno);

    /////
    // Build
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");

    /////
    // Main logic
    clientlen = sizeof(clientaddr);
    while (1) {
        /////
        // Recieve Data
        bzero(buf, BUFSIZE);
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0) { // Sending Handshake Failed
            error("Error in recvFrom");
        } else { // Handshake Established
            /////
            // Determine Sender
            printf("Handshake Established\n");
            hostp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr,
                                  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
            if (hostp == NULL)
                error("ERROR on gethostbyaddr");
            hostaddrp = inet_ntoa(clientaddr.sin_addr);
            if (hostaddrp == NULL)
                error("ERROR on inet_ntoa\n");
            printf("server received datagram from %s (%s)\n",
                   hostp->h_name, hostaddrp);
            /////
            // Parse Client Request
//            printf("Parsing client request\n");
            int sequenceNum;
            int failedAttempts;
            _Bool flags;
            bcopy(&sequenceNum, buf, 4);
            bcopy(&flags, buf + 4, 1);
            char d_buffer[BUF_DATA];
            char data[BUF_DATA];
            char recvFromClient[BUF_DATA];
            char *token;
            char sending_file_name[BUF_DATA];
            bzero(data, BUF_DATA);
            bzero(recvFromClient, BUF_DATA);
            strcpy(data, buf + 5); // was previously data
            long fs = 0;
//            printf("Got to file stuff\n");
            int fileExists = 1;
            FILE *file_to_open = NULL;
            if (data != NULL) {
                file_to_open = fopen(data, "r");
                strcpy(sending_file_name, data);
                if (file_to_open == NULL){
                  fileExists = 0;
                }
                else if (file_to_open) {
                    fseek(file_to_open, 0, SEEK_SET);
//                    printf("%s\n", data);//DEBUG
                } 
                else {
                    fileExists = 0;
                    perror("fopen");
                }
            } 
            else {
                fileExists = 0;
            }
//            printf("Done with file stuff\n");
            bzero(buf, BUFSIZE);

            /////
            // Send Initial Response
            // If error send why and skip to terminate
            // Else confirm connection, then send initial packet
            if (fileExists) {
                fread(d_buffer, BUF_DATA, 1, file_to_open);
            }
//            printf("Finished Fread\n");
            sequenceNum = 1;
            flags = '\0';
            bzero(buf, BUFSIZE);
            buf[0] = (sequenceNum >> 24) & 0xFF;
            buf[1] = (sequenceNum >> 16) & 0xFF;
            buf[2] = (sequenceNum >> 8) & 0xFF;
            buf[3] = (sequenceNum) & 0xFF;
            //bcopy(&sequenceNum, buf, 4);
            bcopy(&flags, buf + 4, 1);
            bcopy(d_buffer, buf + BUF_HEADER, BUF_DATA);
//						for(int i = 0; i < BUFSIZE; ++i){//DEBUG
//								printf("%x ", buf[i]);
//						}
            /////
            // Send Data
            // If recieve response send next packet
            int receivedSequenceNumber = 0;
            int oneMore = 1;
            int foundEOF = 1;
//            printf("Got to sending stuff\n");
            while(!flags) {
                n = sendto(sockfd, buf, BUFSIZE, 0,
                           (struct sockaddr *) &clientaddr, clientlen);
                if ((receivedSequenceNumber) == sequenceNum) {
// UNCOMMENT ME FOR TESTING                    printf("Request for packet %u recieved\n", sequenceNum);
                    sequenceNum++;
                    bzero(buf, BUFSIZE);
                    bzero(d_buffer, BUF_DATA);
                    // PUT HEADER BACK INTO NEW buf
                    buf[0] = (sequenceNum & 0xFF000000) >> 24;
                    buf[1] = (sequenceNum & 0x00FF0000) >> 16;
                    buf[2] = (sequenceNum & 0x0000FF00) >> 8;
                    buf[3] = (sequenceNum) & 0xFF;
                    buf[4] = flags;
//                  printf("Buffer Header set up\n");
                    if (fileExists) {
                        foundEOF = (fread(d_buffer, BUF_DATA, 1, file_to_open) != 0);
                        if (foundEOF | oneMore) {
                            oneMore = foundEOF;
                            //                    printf("fread sucsess\n");
//                            printf("::::::::::\n%s\n::::::::::\n", d_buffer);
                            bcopy(d_buffer, buf + 5, BUF_DATA);
                            //                    printf("BCOPY sucsesful\n");
                        } else {
                            bzero(buf, BUFSIZE);
                            flags = 1;
                            bzero(d_buffer, BUF_DATA);
                            bcopy("FILE TRANSFER COMPLETE", d_buffer, 22);
                            bcopy(d_buffer, buf+BUF_HEADER, BUF_DATA);
                            ++sequenceNum;
                            buf[0] = (sequenceNum & 0xFF000000) >> 24;
                            buf[1] = (sequenceNum & 0x00FF0000) >> 16;
                            buf[2] = (sequenceNum & 0x0000FF00) >> 8;
                            buf[3] = (sequenceNum) & 0xFF;
                            buf[4] = flags;
                        }
                    } else {
                        flags = 1;
                        ++sequenceNum;
                        bzero(d_buffer, BUF_DATA);
                        bcopy("FILE NOT FOUND", d_buffer, 14);
                        buf[0] = (sequenceNum & 0xFF000000) >> 24;
                        buf[1] = (sequenceNum & 0x00FF0000) >> 16;
                        buf[2] = (sequenceNum & 0x0000FF00) >> 8;
                        buf[3] = (sequenceNum) & 0xFF;
                        buf[4] = flags;
                        bcopy(d_buffer, buf+5, BUF_DATA);
                    }
                }
                // printf("BUF:\n %s\n", buf);
                // printf("D_BUF:\n %s\n", d_buffer);
                int x = recvfrom(sockfd, recvFromClient, BUFSIZE, 0,
                                 (struct sockaddr *) &clientaddr, &clientlen);
                if (x > 0) { // Recieved Datagram
                    failedAttempts = 0;
                    bcopy(recvFromClient + 4, &flags, 1);
                    receivedSequenceNumber = (recvFromClient[3] & 0x000000FF);
                    receivedSequenceNumber += ((recvFromClient[2] << 8) & 0x0000FF00);
                    receivedSequenceNumber += ((recvFromClient[1] << 16) & 0x00FF0000);
                    receivedSequenceNumber += ((recvFromClient[0] << 24) & 0xFF000000);
// UNCOMMENT FOR TESTING                    printf("Recieved sequence number: %u\n", receivedSequenceNumber);//DEBUG
                    // flags = recvFromClient[4];
                } else if (x == 0) {// client has shut down.
                    printf("Initiating shutdown...\n");
                    break;
                } else if (x < 0)
                    error("ERROR in recvfrom");
                else {// May have to take this out
                    ++failedAttempts;
                }
                if (failedAttempts > 0xFFF) {
                    error("ERROR to many failed attempts"); // If we have to many atempts to connect
                    break;
                }
            }
            /////
            // Terminate connection
            printf("Terminating connection...\n");
            for (int i = 0; i < 5; ++i) {
                sendto(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, clientlen);
            }
            if (fileExists) {
                fclose(file_to_open);
            }
        }
    }
    return 0;
}
