#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <fstream>
#include <iostream>
#define SERVERPORT "4950" // the port users will be connecting to
#define CLIENTPORT "4960"
#define MAXBUFLEN 100
#define PACKET_DATA_SIZE 500


using namespace std;
struct packet
{
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char  data [PACKET_DATA_SIZE]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet
{
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};


vector<string> parse_in_file(string filename)
{
    ifstream in_file(filename);
    vector<string> file_contents;
    string resline;
    while(getline(in_file, resline))
    {
        file_contents.push_back(resline);
    }
    return file_contents;
}

void write_in_file(string filename, char * char_arr, bool first_open)
{

    ofstream stream;
    if(!first_open)
        stream.open(filename.c_str(), ofstream::out | ofstream::app);
    else{
        stream.open(filename.c_str(),ofstream::in | ofstream::out | ofstream::trunc);

    }
    if( !stream )
        cout << "Opening file failed" << endl;
    // use operator<< for clarity
    stream << char_arr;

    // test if write was succesful - not *really* necessary
    if( !stream )
        cout << "Write failed" << endl;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void bind_clt_socket()
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, CLIENTPORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }
// loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
    }
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    socklen_t addr_len;
    char buf[MAXBUFLEN];
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr;
    int rv;
    int numbytes;

    vector<string> ctnts = parse_in_file("client.in"); //srvip - srvport - cltport - file - window
	string srv_ip = ctnts[0];
	string srv_port = ctnts[1];
	string clt_port = ctnts[2];
	string sendfile = ctnts[3];
	int window_size = stoi(ctnts[4]);
    //bind_clt_socket();
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(srv_ip.c_str(), srv_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
// loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "talker: failed to bind socket\n");
        return 2;
    }
    if ((numbytes = sendto(sockfd, sendfile.c_str(), strlen(sendfile.c_str()), 0,
                           p->ai_addr, p->ai_addrlen)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }
    int cnt = 0;
    bool cond = true;
    while(true)
    {
        addr_len = sizeof their_addr;
        struct packet pkt;
        if ((numbytes = recvfrom(sockfd, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        printf("listener: got packet from %s\n",
               inet_ntop(their_addr.ss_family,
                         get_in_addr((struct sockaddr *)&their_addr),
                         s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);
        pkt.data[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", pkt.data);


        write_in_file(sendfile, pkt.data, cond);
        struct ack_packet ack;
        ack.ackno = cnt++;
        ack.cksum = 0;
        ack.len = 0;
        //sending ack
        printf("sending ack\n");
        if ((numbytes = sendto(sockfd, &ack, sizeof(ack), 0,
                               p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
        printf("ack sent\n");
        cond = false;
    }
    freeaddrinfo(servinfo);
    printf("talker: sent %d bytes to %s\n", numbytes, argv[1]);
    close(sockfd);
    return 0;
}
