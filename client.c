////////////////////////////////////////////////////////////////////////////////
// CLIENT
// NOTES
// Maximum size of a packet we send is 512 bytes
// HEADER FORMAT
// Sequence Number 4 Bytes (0 is reservered for handshake, packet number starts
// at 1)
// Flags 1 Byte (Teardown)
// HANDSHAKE
// First thing send from client to server is file we want
// If we don't recieve a response resend a few times, then error out
// If we recieve a file with a sequence number of zero, it's data field is the
// size of the file we are getting. Allocate that much space.
// RESPONSES
// We are going to start with ping pong, if we recieve a request, reply with
// that requests sequence number
// TEARDOWN
// If we recieve a flag with the teardown flag, respond to the server with the
// teardown flag wait 2 seconds, then do it again. If no response in 8 tries
// assume connection has been closed, teardown
// TODO
// send handshake, wait for 2 seconds, if reply begin requesting,
// if no reply resend up to 8 times
// if error reply print error
// reassemble file into the local directory, of the name the user requested
// TODO Eventualy
// Keep track of what data we have already recieved, and request the smallest
// sequence number that we are missing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>

#define BUFSIZE 512
#define BUF_DATA 506
#define BUF_HEADER 5

char sendBuf[BUFSIZE];

void error(char *msg) {
    perror(msg);
    //exit(0);
}

void makeBuffer(int sequenceNum, _Bool flags, char data[BUF_DATA]) {
    int i = 4;
    int offset = BUFSIZE - BUF_DATA;
    bcopy(&sequenceNum, sendBuf, 4);
    bcopy(&flags, sendBuf + 4, 1);
    bcopy(data, sendBuf + 5, BUF_DATA);
    sendBuf[0] = (sequenceNum >> 24) & 0xFF;
    sendBuf[1] = (sequenceNum >> 16) & 0xFF;
    sendBuf[2] = (sequenceNum >> 8) & 0xFF;
    sendBuf[3] = sequenceNum & 0xFF;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int clientlen, serverlen;
    struct sockaddr_in serveraddr;
    struct sockaddr_in clientaddr;
    struct hostent *hostp;
    struct hostent *server;
    char *hostname;
    char recvBuf[BUFSIZE];
    char *hostaddrp;
    int optval;
    char *filename;
    char *storageLoc;
    char sendData[BUF_DATA];
    int failedAttempts;
    /////
    // Input
    if (argc != 5) {
        fprintf(stderr, "usage: %s <hostname> <port> <file to request> <storage location>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];
    storageLoc = argv[4];
    printf("Requested file %s, storing at %s\n", filename, storageLoc);
    FILE *file = fopen(storageLoc, "w+");
    /////
    // Setup socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    /////
    // Set timeout
    struct timeval tv;
    tv.tv_sec = 2; // Timeout in seconds
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (struct timeval *) &tv, sizeof(struct timeval));

    /////
    // Host Info
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /////
    // Server Info
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serveraddr.sin_addr.s_addr,
          server->h_length);//FIXME inet_addr(Server_IP)
    serveraddr.sin_port = htons(portno);

    /////
    // Initial Setup
    int sequenceNum = 0;
    int recvSeqNum = 0;
    _Bool flags = 0;
    bzero(sendData, BUF_DATA);

    /////
    // Format Initial Request
    bzero(sendBuf, BUFSIZE);
    makeBuffer(sequenceNum, flags, filename);

    /////
    // Send Initial Request
    serverlen = sizeof(serveraddr);
    char handshakeLoop = 8;
    char timesFailed = 0;
    for (int i = 0; i < handshakeLoop; ++i) {
        n = sendto(sockfd, sendBuf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");
        n = recvfrom(sockfd, recvBuf, BUFSIZE, 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
// UNCOMMENT FOR RESTING        printf("recieve returned: %i\n", n);//DEBUG
        if (n > 0) {
            printf("Connection Established...\n");
            break;
        } else if (n == 0) {
            printf("Server has shutdown\n");
            break;
        } else if (n < 0) {
            error("ERROR recieving response");
            ++timesFailed;
        }
        if (timesFailed >= handshakeLoop) { // Sending Handshake Failed
            error("ERROR to many failed requests for handshake\n");
            return -1;
        }
    }
    bzero(sendData, BUF_DATA);
    makeBuffer(sequenceNum, flags, sendData);
    bzero(recvBuf, BUFSIZE);
    /////
    // Handshake successfully established
    // while (flags != 1){ // While we have not recieved the terminate command
    failedAttempts = 0;
    while (!flags) {
        //printf("%d\n", i);
        n = recvfrom(sockfd, recvBuf, BUFSIZE, 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
// UNCOMMENT FOR TESTING        printf("Reciever returned %i\n", n);//DEBUG
        if (n > 0) { // Recieved Datagram
            //printf("%i\n", recvSeqNum); //TEMPORARY
            failedAttempts = 0;
            bcopy(recvBuf + 4, &flags, 1);
            recvSeqNum = (recvBuf[3]) & 0x000000FF;
            recvSeqNum += (recvBuf[2] << 8) & 0x0000FF00;
            recvSeqNum += (recvBuf[1] << 16) & 0x00FF0000;
            recvSeqNum += (recvBuf[0] << 24) & 0xFF000000;
// UNCOMMENT FOR TESTING            printf("Recieved sequence Number: %u\n", recvSeqNum);//DEBUG
            flags = recvBuf[4];
            if (recvSeqNum == sequenceNum + 1) {
                ++sequenceNum;
                //fwrite(recvBuf+BUF_HEADER, BUF_HEADER, 1, file);
                fputs(recvBuf + BUF_HEADER, file);
            }
// UNCOMMENT FOR TESTING            printf("Sending sequence number %u\n", sequenceNum);
            makeBuffer(sequenceNum, flags, sendData);
        } else if (n == 0) {// no message recieved, peer has shutdown
            printf("Initiating shutdown...\n");
            failedAttempts == 0;
            break;
        } else {// Probably should take this out, replace with something that looks for timeouts
            ++failedAttempts;
        }
        if (failedAttempts > 0xFFF) {
            error("ERROR too many failed attempts"); // If we have more than alot of attempts to connect
            break;
        }
        n = sendto(sockfd, sendBuf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, serverlen);
        if (n < 0)
            error("Error in sendto");
    }
    printf("Connection Terminated:\n%s\n", recvBuf+5);
    fclose(file);
    return 0;
}
