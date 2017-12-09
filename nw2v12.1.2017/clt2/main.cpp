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
#include <string.h>
#define SERVERPORT "4950" // the port users will be connecting to
#define CLIENTPORT "4960"
#define MAXBUFLEN 100
#define PACKET_DATA_SIZE 500

using namespace std;
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    socklen_t addr_len;
    char buf[MAXBUFLEN];
    char s[INET6_ADDRSTRLEN];
    struct sockaddr_storage their_addr;
    int rv;
    int numbytes;
    string sendfile;


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

struct packet_sr{
    struct packet pkt;
    bool received;
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

void write_image_file(string filename, char* buf, int length, bool append) {
    ofstream file;
    if(!append) {
        file.open(filename.c_str(), ios::out | ios::binary | ios::app);
    } else {
        file.open(filename.c_str(), ios::out | ios::binary);
    }
    cout << "write image file length :"<<length<< endl;
    cout <<  buf<<endl;
    file.write(buf, length);
    file.close();
}
void write_txt_file(string filename, char * char_arr, bool first_open)
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

void write_in_file(string filename, char*buf, int length, bool first_open){
    if(filename.substr(filename.find_last_of(".") + 1) == "txt"){
		write_txt_file(filename, buf, first_open);
	}
	else{
        cout << "write image call" << endl;
		write_image_file(filename, buf, length, first_open);

	}
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
void stop_and_wait(){
    int cnt = 0;
    bool cond = true;
    int numbytes;
    char s[INET6_ADDRSTRLEN];

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
        //pkt.data[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", pkt.data);

        cout << "pkt.len" << pkt.len << endl;
        write_in_file(sendfile, pkt.data, (int)pkt.len, cond);
        struct ack_packet ack;
        ack.ackno = pkt.seqno;
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


}

void selective_repeat (int window_size){
       int cnt = 0;
    bool cond = true;
    int numbytes;
    char s[INET6_ADDRSTRLEN];
    packet_sr pkts_sr[1000];
    int rcv_base = 0;
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
        int rcv_pkt_sr_seqno = (int)pkt.seqno;
        cout << "rcv_pkt_sr_seqno :" << rcv_pkt_sr_seqno << endl;
        if(rcv_pkt_sr_seqno > rcv_base+window_size-1 || rcv_pkt_sr_seqno < rcv_base-window_size){
            cout << "errrrrrrrr"<<endl;
            continue;
        }
        pkts_sr[rcv_pkt_sr_seqno].pkt  = pkt;
        pkts_sr[rcv_pkt_sr_seqno].received = true;


        printf("listener: got packet from %s\n",
               inet_ntop(their_addr.ss_family,
                         get_in_addr((struct sockaddr *)&their_addr),
                         s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);
        pkt.data[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", pkt.data);


        struct ack_packet ack;
        ack.ackno = pkt.seqno;
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

        //writing inorder packet in file
        if(rcv_pkt_sr_seqno == rcv_base){
            while(pkts_sr[rcv_base].received){

                write_in_file(sendfile, pkts_sr[rcv_base].pkt.data, pkts_sr[rcv_base].pkt.len, cond);
                cond = false;

                rcv_base++;
            }




        }
        freeaddrinfo(servinfo);

    }
}

void go_back_N(int window_size){
    bool cond = true;
    int numbytes;
    char s[INET6_ADDRSTRLEN];
    int expected_seqno = 0;
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
        //pkt.data[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", pkt.data);
        struct ack_packet ack;
        ack.cksum = 0;
        ack.len = 0;
        /*if pkt SEQNO is equal to expected_seqno -> return ack
                                                  -> write pkt in file

          if pkt.seqno not equal to seqno -> return ack equal to last received one
        */
        if(expected_seqno == (int)pkt.seqno){
            write_in_file(sendfile, pkt.data, pkt.len, cond);
            ack.ackno = pkt.seqno;
            cond = false;
                //sending ack
        }
       else{
            ack.ackno = expected_seqno-1;

       }
       printf("sending ack\n");
       if ((numbytes = sendto(sockfd, &ack, sizeof(ack), 0,
                                       p->ai_addr, p->ai_addrlen)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }
        printf("ack sent\n");

    }
    freeaddrinfo(servinfo);

}
int main(int argc, char *argv[])
{

    vector<string> ctnts = parse_in_file("client.in"); //srvip - srvport - cltport - file - window
	string srv_ip = ctnts[0];
	string srv_port = ctnts[1];
	string clt_port = ctnts[2];
	sendfile = ctnts[3];
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
   stop_and_wait();
   // selective_repeat(sockfd, addr_len, their_addr, sendfile, servinfo, p, window_size);
    close(sockfd);
    return 0;
}
