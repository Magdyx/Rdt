/*
** listener.c -- a datagram sockets "server" demo
*/#include <stdio.h>
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
#include <cstdio>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

#define MYPORT "4950" // the port users will be connecting to
#define MAXBUFLEN 100
#define SRV_IN_FILE "server.in"
#define PACKET_DATA_SIZE 500
#define WINDOW_SIZE 5
#define TIME_OUT 3.0

using namespace std;
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
int numbytes;
socklen_t addr_len;
struct sockaddr_storage their_addr;
char buf[MAXBUFLEN];
char s[INET6_ADDRSTRLEN];
struct packet
{
    /* Header */
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data[PACKET_DATA_SIZE ]; /* Not always 500 bytes, can be less */
};
struct packet_sr
{
    struct packet pkt;
    bool acked;
    clock_t start_time;
};

/* Ack-only packets are only 8 bytes */
struct ack_packet
{
    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t ackno;
};

int  break_file(struct packet* data_pkts, char* buf, int len)
{
    int pkts_cnt, bytes_left = len;
    char* buf_iterator;


    buf_iterator = buf;
    pkts_cnt = len / (PACKET_DATA_SIZE-1);
    if (pkts_cnt * (PACKET_DATA_SIZE-1) < len) ++pkts_cnt; //ceil integer division

    cout << "buffer in break_file :" << buf << endl;
    for(int i=0; i<pkts_cnt; i++)
    {
        int packet_size = PACKET_DATA_SIZE-1;
        if (bytes_left < PACKET_DATA_SIZE-1)
        {
            packet_size = bytes_left;
        }
        memset(data_pkts[i].data, '\0', sizeof(data_pkts[i].data));
        cout << "break_file buf_iterator: " << buf_iterator << endl;
        strncpy(data_pkts[i].data, buf_iterator, packet_size);

        //cout << data_pkts[i].data << endl;
        data_pkts[i].cksum = 0;
        data_pkts[i].seqno = i;
        data_pkts[i].len = packet_size;

        buf_iterator += packet_size;
        bytes_left -=  packet_size;
    }
    //cout << "pkts size" << pkts_cnt <<endl;
    return pkts_cnt;
}


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
        stream.open(filename, ofstream::out | ofstream::app);
    else
        stream.open(filename);
    if( !stream )
        cout << "Opening file failed" << endl;
    // use operator<< for clarity
    stream << char_arr;
    // test if write was succesful - not *really* necessary
    if( !stream )
        cout << "Write failed" << endl;
}
int read_from_file(char * buf, string filename)
{
    std::ifstream is (filename.c_str(), std::ifstream::binary);
    int length = 0;
    if (is)
    {
        // get length of file:
        memset(buf, '\0', sizeof(buf));
        is.seekg (0, is.end);
        length = is.tellg();
        is.seekg (0, is.beg);

        char * buffer = new char [length];


        // read data as a block:
        is.read (buffer,length);

        if(!is)
            cout << "error: only " << is.gcount() << " could be read"<<endl;
        strncpy(buf, buffer, length * sizeof(char));
        is.close();

        // ...buffer contains the entire file...

        delete[] buffer;
    }
    else
    {
        cout << "error";
    }
    cout << "read_from_file buffer :";
    buf[length-2] = '\0';
    cout << buf<<endl;
    return length;
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

void selective_repeat(int window_size)
{
    char  txt_in_file [10000];
    int length = read_from_file(txt_in_file, "file.txt");

    cout << "read from file"<<endl;
    cout << "read_from_file :" << txt_in_file << endl;
    int base = 0;

    struct packet pkts [1000];
    struct packet_sr pkts_sr [1000];
    int pkts_length = break_file(pkts, txt_in_file, length);
    for(int i = 0; i < pkts_length; i++)
    {
        pkts_sr[i].pkt = pkts[i];
        pkts_sr[i].acked = false;
        pkts_sr[i].start_time = 0;
    }

    for(int i = 0; i < pkts_length; i++)
    {

        //set packets_sr;
        if(i < base+window_size)
        {
            if ((numbytes = sendto(sockfd, &pkts_sr[i].pkt, sizeof(pkts_sr[i].pkt), 0,
                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }
        }
        pkts_sr[i].start_time = clock();

        //receiving ack
        addr_len = sizeof their_addr;
        struct ack_packet ack;
        printf("ack receiving\n");
        if ((numbytes = recvfrom(sockfd, &ack,sizeof(ack), 0,
                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);

        }
        pkts_sr[ack.ackno].acked = true;
        //receiving ack with the same as base:
        printf("ack received\n");
        printf("ack no : %d\n",ack.ackno);


        if((int)ack.ackno == base)
        {
            while(pkts_sr[base].acked)
            {
                base++;
            }
        }
        else
        {
            for(int j = base; j < ack.ackno; j++)
            {
                double duration = (double)(  clock() - pkts_sr[j].start_time)/ CLOCKS_PER_SEC;
                if(!pkts_sr[j].acked && duration > TIME_OUT)
                {

                    if ((numbytes = sendto(sockfd, &pkts_sr[j].pkt, sizeof(pkts_sr[j].pkt), 0,
                                           (struct sockaddr *)&their_addr, addr_len)) == -1)
                    {
                        perror("talker: sendto");
                        exit(1);
                    }
                    pkts_sr[j].start_time = clock();
                }
            }
        }

        //    if(ack.ackno != i)
        //      i--;
    }

    cout << "buf sent back from server" << endl;

}

void selective_repeat_with_congition_control(int window_size)
{
    char  txt_in_file [10000];
    int dup_acks = 0;
    int length = read_from_file(txt_in_file, "file.txt");
    int cwnd = 1,ssthd = -1;
    cout << "read from file"<<endl;
    cout << "read_from_file :" << txt_in_file << endl;
    int base = 0;

    struct packet pkts [1000];
    struct packet_sr pkts_sr [1000];
    int pkts_length = break_file(pkts, txt_in_file, length);
    for(int i = 0; i < pkts_length; i++)
    {
        pkts_sr[i].pkt = pkts[i];
        pkts_sr[i].acked = false;
        pkts_sr[i].start_time = 0;
    }
    int i = 0;//i represent seqno
    while(i < pkts_length)
    {
        int end_of_loop = 0;
        if(ssthd == -1){
            end_of_loop = min(window_size, cwnd);
        }
        else{
            end_of_loop = min(min(window_size, cwnd), ssthd);
        }


        //set packets_sr;
        for(int j = 0; j < end_of_loop; j++)
        {
            if ((numbytes = sendto(sockfd, &pkts_sr[i+j].pkt, sizeof(pkts_sr[i+j].pkt), 0,
                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }

            pkts_sr[i+j].start_time = clock();
        }


        //receiving ack

        for(int j = 0; j < end_of_loop; j++)
        {
            addr_len = sizeof their_addr;
            struct ack_packet ack;
            printf("ack receiving\n");

            //timout handling using select
            fd_set fds;
            int n;
            struct timeval tv;
            FD_ZERO(&fds);
            FD_SET(sockfd, &fds);

            tv.tv_sec = 2;
            tv.tv_usec = 0;

            n = select(sockfd+1, &fds, NULL, NULL, &tv);
            if(n == 0)
            {
                //timeout

                //1- update ssthd
                if(ssthd == -1)
                    ssthd = cwnd/2;
                else
                    ssthd = min(ssthd, cwnd/2);
                //2 - change the seqno with first unsend packet in buffer
                i = i + j;

                //3- update cwnd
                cwnd = 1;
                break;

            }
            else if(n == -1)
            {
                //error
                perror("recev error");
            }
            else{
                if ((numbytes = recvfrom(sockfd, &ack,sizeof(ack), 0,
                                         (struct sockaddr *)&their_addr, &addr_len)) == -1)
                {
                    perror("recvfrom");
                    exit(1);

                }
                pkts_sr[ack.ackno].acked = true;

                printf("ack received\n");
                printf("ack no : %d\n",ack.ackno);

                //receiving ack with the same as base:
                if((int)ack.ackno == base)
                {
                    while(pkts_sr[base].acked)
                    {
                        base++;
                    }
                }
                else
                {

                    for(int k = base; k < ack.ackno; k++)
                    {
                        double duration = (double)(  clock() - pkts_sr[k].start_time)/ CLOCKS_PER_SEC;
                        if(!pkts_sr[k].acked && duration > TIME_OUT)
                        {

                            if ((numbytes = sendto(sockfd, &pkts_sr[k].pkt, sizeof(pkts_sr[k].pkt), 0,
                                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
                            {
                                perror("talker: sendto");
                                exit(1);
                            }
                            pkts_sr[k].start_time = clock();
                        }
                    }
                }

            }
            i += cwnd;
            cwnd = cwnd*2;

        }

        //kolo tmama

    }

    cout << "buf sent back from server" << endl;

}


bool take_it(int file_prob)
{
    int prob = rand() % 100;
    file_prob = 100 - file_prob;
    if(prob < file_prob)
        return true;
    return false;
}

void stop_wait (int file_prob)
{
    int numbytes = 0;
    char  txt_in_file [10000];
    int length = read_from_file(txt_in_file, "file.txt");

    cout << "read from file"<<endl;


    cout << "read_from_file :" << txt_in_file << endl;
    struct packet pkts [100];
    int pkts_length = break_file(pkts, txt_in_file, length);
    cout <<"pkts_length:" <<pkts_length;
    cout << "pkt 0 data:" <<pkts[0].data << endl;
    cout << "pkt0data:" <<pkts[0].data << endl;
    for(int i = 0; i < pkts_length; i++)
    {

        if ( take_it(file_prob) && (numbytes = sendto(sockfd, &pkts[i], sizeof(pkts[i]), 0,
                                               (struct sockaddr *)&their_addr, addr_len)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }

        //receiving ack
        addr_len = sizeof their_addr;
        struct ack_packet ack;
        printf("ack receiving\n");

        //timout handling using select
        fd_set fds;
        int n;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        tv.tv_sec = 2;
        tv.tv_usec = 0;

        n = select(sockfd+1, &fds, NULL, NULL, &tv);
        if(n == 0)
        {
            //timeout
            i--;
            continue;
        }
        else if(n == -1)
        {
            //error
            perror("recev error");
        }
        else if ((numbytes = recvfrom(sockfd, &ack,sizeof(ack), 0,
                                      (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);

        }
        printf("ack received\n");
        printf("ack no : %d\n",ack.ackno);
        if(ack.ackno != i)
            i--;
    }

    cout << "buf sent back from server" << endl;

}

void go_back_N(int window_size)
{
    char  txt_in_file [10000];
    int length = read_from_file(txt_in_file, "file.txt");

    cout << "read from file"<<endl;
    cout << "read_from_file :" << txt_in_file << endl;
    int base = 0;
    clock_t start;
    struct packet pkts [1000];
    int pkts_length = break_file(pkts, txt_in_file, length);

    for(int i = 0; i < pkts_length; i++)
    {

        //set packets_sr;
        if(i < base+window_size)
        {
            if ((numbytes = sendto(sockfd, &pkts[i].pkt, sizeof(pkts[i].pkt), 0,
                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }

            if( i == base)
                   start  = clock();

        }



        //receiving ack
        addr_len = sizeof their_addr;
        struct ack_packet ack;

        //receiving ack with the same as base:
         double duration = (double)(  clock() - pkts[j].start_time)/ CLOCKS_PER_SEC;
        if(duration > TIME_OUT)
        {//timout
            for(int k = base; k < i; k++){
                if ((numbytes = sendto(sockfd, &pkts[i].pkt, sizeof(pkts[i].pkt), 0,
                                   (struct sockaddr *)&their_addr, addr_len)) == -1)
                {
                    perror("talker: sendto");
                    exit(1);
                }
                if( i == base)
                   start  = clock();
            }
        }



        if ((numbytes = recvfrom(sockfd, &ack,sizeof(ack), 0,
                                      (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);

        }
        base = ack.ackno+1;
        if(base <= i){
            start = clock();
        }
        printf("ack received\n");
        printf("ack no : %d\n",ack.ackno);

    }

    cout << "buf sent back from server" << endl;

}


int main(void)
{



    vector<string> fctnts = parse_in_file(SRV_IN_FILE);
    string srv_port = fctnts[0];
    int window_size = stoi(fctnts[1]);
    int seed = stoi(fctnts[2]);
    float success_prob = stof(fctnts[3]) * 100;

    srand(seed);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, srv_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
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
        return 2;
    }
    freeaddrinfo(servinfo);
    printf("listener: waiting to recvfrom...\n");
    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
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
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
    cout << "sending back" << endl;

    stop_wait((int)success_prob);
    //selective_repeat(window_size);
    close(sockfd);
    return 0;
}
