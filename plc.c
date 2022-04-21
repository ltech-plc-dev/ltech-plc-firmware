#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 

#include <pthread.h>
#include <stdbool.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/utsname.h>

void * connection_handler(void *sock_desc);
const char intro_message[] = "\n[LT] Welcome to the LT PLC\n[LT] Type HELP to see available commands.\n\n";
int g_can_socket_connected = 0;
int can_socket;
volatile bool client_connected;

int can_do_read(struct can_frame *frame)
{
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct timeval tv;

    int nbytes = -1;
    if(!g_can_socket_connected)
    {
        can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if(can_socket < 0)
        {
            return -1;
        }
        strcpy(ifr.ifr_name, "vcan0");
        ioctl(can_socket, SIOCGIFINDEX, &ifr);
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        nbytes = bind(can_socket, (struct sockaddr*)&addr, sizeof(addr));
        if(nbytes < 0)
        {
            return -1;
        }
        tv.tv_sec = 4;
        tv.tv_usec = 0;
        setsockopt(can_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        g_can_socket_connected = 1;
    }

    nbytes = read(can_socket, frame, sizeof(struct can_frame));
    return nbytes;
}

int do_can_write(struct can_frame *frame)
{
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct timeval tv;
    int nbytes = -1;
    if(!g_can_socket_connected)
    {
        can_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if(can_socket < 0)
        {
            return -1;
        }
        strcpy(ifr.ifr_name, "vcan0");
        if( 0 > ioctl(can_socket, SIOCGIFINDEX, &ifr) )
        {
            return -1;
        }
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        nbytes = bind(can_socket, (struct sockaddr*)&addr, sizeof(addr));
        if(nbytes < 0)
        {
            return -1;
        }
        tv.tv_sec = 4;
        tv.tv_usec = 0;
        setsockopt(can_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        g_can_socket_connected = 1;
    }

    nbytes = write(can_socket, frame, sizeof(struct can_frame));

    return nbytes;
}

void print_plc_status(int connfd)
{
    struct can_frame frame;
    int nbytes = 0;
    char sendBuff[1024] = {0};
    
    frame.can_id = 0xabc;
    frame.can_dlc = 0x05;
    frame.data[0] = 0xa1;
    frame.data[1] = 0xb2;
    frame.data[2] = 0xc3;
    frame.data[3] = 0xd4;
    frame.data[4] = 0xe5;

    nbytes = do_can_write(&frame);
    if(nbytes < 0)
    {
        write(connfd, "\n[LT] Unable to connect to PLC!\n\n", 33);
        return;
    }
    nbytes = can_do_read(&frame);
    if(nbytes < 0)
    {
        write(connfd, "\n[LT] No response received from PLC!\n\n", 38);
        return;
    }

    write(connfd, "\n PLC Status:\n",14);

    snprintf(sendBuff, sizeof(sendBuff), "  - PLC Version:\t%d.%d\n", (frame.data[1] & 0xf0) >> 4, frame.data[1] & 0x0f);
    write(connfd, sendBuff, strlen(sendBuff));
    if (frame.data[2] == 0)
        write(connfd, "  - Status:\t\tStopped\n", 21);
    else if (frame.data[2] == 2)
        write(connfd, "  - Status:\t\tRunning\n", 21);
    else
        write(connfd, "  - Status:\t\tCrashed\n", 21);

    write(connfd, "\n", 1);
}

void print_commands(int connfd)
{
    write(connfd, "[LT] Available Commands:\n", 25);
    write(connfd, "      HELP\n", 11);
    write(connfd, "      STATUS\n", 13);
    write(connfd, "      EXIT\n\n", 12);
}

void *connection_handler(void *socket_desc)
{
    int connfd = *((int*)socket_desc);
    char sendBuff[1024] = {0};
    char client_message[1024];

    write(connfd, intro_message , 71);
    print_commands(connfd);

    while( ( recv(connfd , client_message , 1024 , 0)) > 0 )
    {
        if (strncasecmp(client_message, "HELP", 4) == 0)
        {
            write(connfd, "\n     + HELP        - displays this output\n", 43);
            write(connfd, "     + STATUS      - displays status of PLC\n", 63);
            write(connfd, "     + EXIT        - disconnects from PLC\n\n", 43);
            continue;
        }
        else if (strncasecmp(client_message, "STATUS", 4) == 0)
        {
            print_plc_status(connfd);
            continue;
        }
        else if (strncasecmp(client_message, "EXIT", 4) == 0)
        {
            write(connfd, "[LT] Bye\n\n", 9); 
            close(connfd);
            continue;
        }
        write(connfd, "[LT] Invalid Command Provided\n\n", 31);
        print_commands(connfd);
    }
    
    puts("Client disconnected");
    fflush(stdout);
    close(connfd);
    client_connected = false;

    return 0;
}

int main(int argc, char *argv[])
{
    int socket_desc , client_sock , c;
    struct sockaddr_in server , client;

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8080 );
     
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("bind failed. Error");
        return 1;
    }
    puts("bind done");
     
    listen(socket_desc , 3);
     
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);
    
    pthread_t thread_id;
    
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted");
         
        if( pthread_create( &thread_id , NULL ,  connection_handler , (void*) &client_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }
        puts("Handler assigned");
    }
     
    if (client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }
     
    return 0;
}
