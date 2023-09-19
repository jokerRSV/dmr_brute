/*-------------------------------------------------------------------------------
 * dsd_rigctl.c
 * Simple RIGCTL Client for DSD (remote control of GQRX, SDR++, etc)
 *
 * Portions from https://github.com/neural75/gqrx-scanner
 *
 * LWVMOBILE
 * 2022-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSIZE         1024
void error(char *msg) {
    perror(msg);
    exit(0);
}

//
// Connect
//
int Connect(char *hostname, int portno) {
    int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;


    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR opening socket\n");
        error("ERROR opening socket");
    }


    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        //exit(0);
        return (0); //return 0, check on other end and configure pulse input 
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr,
          (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        fprintf(stderr, "ERROR opening socket\n");
        return (0);
    }

    return sockfd;
}
//
// Send
//
bool Send(int sockfd, char *buf) {
    int n;

    n = write(sockfd, buf, strlen(buf));
    if (n < 0)
        error("ERROR writing to socket");
    return true;
}

//
// Recv
//
bool Recv(int sockfd, char *buf) {
    int n;

    n = read(sockfd, buf, BUFSIZE);
    if (n < 0)
        error("ERROR reading from socket");
    buf[n] = '\0';
    return true;
}


//
// GQRX Protocol
//
long int GetCurrentFreq(int sockfd) {
    long int freq = 0;
    char buf[BUFSIZE];
    char *ptr;
    char *token;

    Send(sockfd, "f\n");
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return freq;

    token = strtok(buf, "\n");
    freq = strtol(token, &ptr, 10);
    // fprintf (stderr, "\nRIGCTL VFO Freq: [%ld]\n", freq);
    return freq;
}

bool SetFreq(int sockfd, long int freq) {
    char buf[BUFSIZE];

    sprintf(buf, "F %ld\n", freq);
    Send(sockfd, buf);
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return false;

    return true;
}

bool SetModulation(int sockfd, int bandwidth) {
    char buf[BUFSIZE];
    //the bandwidth is now a user/system based configurable variable
    sprintf(buf, "M FM %d\n", bandwidth);
    Send(sockfd, buf);
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return false;

    return true;
}
