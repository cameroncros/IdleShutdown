#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <linux/ethtool.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <poll.h>
#include <inttypes.h>
#include <time.h>
#include "main.h"

void packetdump(char* buffer, int buffer_len, uint16_t* srcport, uint16_t* destport)
{
    struct ethhdr *eth = (struct ethhdr *)(buffer);
#ifdef _DEBUG
    printf("\nEthernet Header\n");
    printf("\t|-Source Address: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
    printf("\t|-Destination Address: %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
    printf("\t|-Protocol: %d\n",eth->h_proto);
#endif

    struct sockaddr_in source, dest;
    uint16_t iphdrlen;
    struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
    memset(&source, 0, sizeof(source));
    source.sin_addr.s_addr = ip->saddr;
    memset(&dest, 0, sizeof(dest));
    dest.sin_addr.s_addr = ip->daddr;

#ifdef _DEBUG 
    printf("\nIP Header\n");
    printf("\t|-Version: %d\n", (unsigned int)ip->version);
    printf("\t|-Internet Header Length: %d DWORDS or %d Bytes\n", (unsigned int)ip->ihl, ((unsigned int)(ip->ihl))*4);
    printf("\t|-Type Of Service: %d\n", (unsigned int)ip->tos);
    printf("\t|-Total Length: %d Bytes\n", ntohs(ip->tot_len));
    printf("\t|-Identification: %d\n", ntohs(ip->id));
    printf("\t|-Time To Live: %d\n", (unsigned int)ip->ttl);
    printf("\t|-Protocol: %d\n", (unsigned int)ip->protocol);
    printf("\t|-Header Checksum: %d\n", ntohs(ip->check));
    printf("\t|-Source IP: %s\n", inet_ntoa(source.sin_addr));
    printf("\t|-Destination IP: %s\n", inet_ntoa(dest.sin_addr));
#endif

    /* getting actual size of IP header*/
    iphdrlen = ip->ihl*4;
    /* getting pointer to tcp header*/
    struct tcphdr *tcp=(struct tcphdr*)(buffer + iphdrlen + sizeof(struct ethhdr));

    *destport = ntohs(tcp->source);
    *srcport = ntohs(tcp->dest);

#ifdef _DEBUG
    printf("\nTCP Header\n");
    printf("\t|-Source Port: %d\n", *srcport);
    printf("\t|-Destination Port: %d\n", *destport);
    printf("\n\n");
#endif
}

void printreport(PJOB job)
{
    printf("\nPorts:\n");
    for (int i = 0; i < MAX_PORTS; i++)
    {
        if (job->ports[i] != 0 && job->ports[i] != -1) {
            printf("%i:\t%i\n", i, job->ports[i]);

        }
    }
    printf("\n");
}

void printhelp(char* exe)
{
    printf("Usage: %s\n", exe);
    printf("\t--timeout {timeout}\n");
    printf("\t--command {command}\n");
    printf("\t--ports {ports}\n");
}

void parseports(char* portstr, PJOB job)
{
    int i = 0;
    while (portstr[i] != '\0')
    {
        // Seek to start of next number.
        while (portstr[i] < '0' || portstr[i] > '9') {
            i++;
            continue;
        }

        // Read next number.
        uint16_t val = 0;
        sscanf(portstr + i, "%" SCNd16, &val);
        if (val > 0 && val <= MAX_PORTS)
        {
            job->ports[val] = 0;
        }
        
        // Seel to end of number.
        while (portstr[i] >= '0' && portstr[i] <= '9') i++;
    }
}

void parseargs(int argc, char** argv, PJOB job)
{
    // Initialise all ports to unwatched.
    for (int i = 0; i < MAX_PORTS; i++)
    {
        job->ports[i] = -1;
    }

    // Parse arguments
    int i = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strncmp("--timeout", argv[i], 9) == 0) {
            i++;
            sscanf(argv[i], "%i", &job->timeout);
        } else if (strncmp("--command", argv[i], 9) == 0) {
            i++;
            job->command = argv[i];
        } else if (strncmp("--ports", argv[i], 7) == 0) {
            i++;
            parseports(argv[i], job);
        } else {
            printf("Failed to parse argument [%s]\n", argv[i]);
            printhelp(argv[0]);
            exit(0);
        }
    }
}


void waitfortraffic(int sock, PJOB job)
{
    time_t now;
    time(&now);

    time_t end = now + job->timeout;
    
    for (; now < end; time(&now))
    {
        // Poll for new data
        struct pollfd fds[1];
        fds[0].fd = sock;
        fds[0].events = POLLIN;
        int timeout_ms = 1000;
        int ret = poll(fds, 1, timeout_ms);
        if (ret == 0)
        {
            continue;
        }

        char buffer[BUFFER_LEN] = { 0 };
        struct sockaddr_in src_addr;
        socklen_t src_addr_len;
        int length_received = recvfrom(sock, buffer, BUFFER_LEN, 0, (struct sockaddr*)&src_addr, &src_addr_len);
        if (length_received == -1)
        {
            perror("recvfrom() failed:");
            exit(errno);
        }
        uint16_t src, dest;
        packetdump(buffer, length_received, &src, &dest);
         
        if (job->ports[src] != -1) {
            printf("Traffic seen on port %i\n", src);
            time(&now);
            end = now + job->timeout;
        }
        if (job->ports[dest] != -1) {
            printf("Traffic seen on port %i\n", dest);
            time(&now);
            end = now + job->timeout;
        }
    }
    if (job->command)
    {
        system(job->command);
    }
}


int main(int argc, char** argv)
{
    JOB job = { 0 };
    parseargs(argc, argv, &job);

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock <= 0)
    {
        perror("Create socket failed: ");
        exit(errno);
    }
    
    waitfortraffic(sock, &job);

    return 0;
}
