/**
  * Author : Simran Dhawan 
  * Student Id: 7700011429 
  * Created: 02/26/2023
  * File name: client.c
**/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* Timeout for receiving response */
#define TIMEOUT                        (3)

/* Protocol Primitives */
#define START_PACKET                   (0xFFFF)
#define END_PACKET                     (0xFFFF)
#define MAX_CLIENT_ID                  (0xFF)
#define MAX_LENGTH                     (0xFF)
#define MAX_SOURCE_SUBSCRIBER_NUM      (0xFFFFFFFF)

/* Technology supported */
#define TECHNOLOGY_2G                  (2)
#define TECHNOLOGY_3G                  (3)
#define TECHNOLOGY_4G                  (4)
#define TECHNOLOGY_5G                  (5)

/* Request Packet Identifier*/
#define ACCESS_REQUEST                 (0XFFF8)

/* Response Packet Identifier */
#define NOT_PAID                       (0XFFF9)
#define NOT_EXIST                      (0XFFFA)
#define ACCESS_OK                      (0XFFFB)

int client_idx = 0x01 ; 

/* Request Packet Format */
struct RequestPacket{
    uint16_t start_packet_id;
    uint8_t client_id;
    uint16_t acc_per;
    uint8_t segment_no;
    uint8_t length;
    uint8_t technology;
    uint32_t source_subscriber_number;
    uint16_t end_packet_id;
};

/* Response packet Format 
   ok_not_paid_exist field identifies if access granted or number does not exist or not paid */
struct ResponsePacket{
    uint16_t start_packet_id;
    uint8_t client_id;
    uint16_t ok_not_paid_exist;
    uint8_t segment_no;
    uint8_t length;
    uint8_t technology;
    uint32_t source_subscriber_number;
    uint16_t end_packet_id;
};

/* Create Request packet */
void createPkt(struct RequestPacket* request_packet, uint8_t* segment_idx){
    request_packet->start_packet_id = START_PACKET;
    request_packet->client_id = client_idx;
    request_packet->acc_per = ACCESS_REQUEST;
    request_packet->segment_no = *segment_idx;
    *segment_idx = (*segment_idx + 1) % 0xFF;
    request_packet->end_packet_id = END_PACKET;  
}

/* Print Request Packet */
void printRequestPacket(struct RequestPacket* request_packet){
    printf("Request Packet is: \n");
    printf("Start Packet Id: 0x%x \n", request_packet->start_packet_id);
    printf("Client Id: 0x%x \n", request_packet->client_id);
    printf("Accesss permission: 0x%x \n", request_packet->acc_per);
    printf("Segment Idx: 0x%x \n", request_packet->segment_no);
    printf("Packet Length: 0x%x \n", request_packet->length);
    printf("Technology: 0x%x \n", request_packet->technology);
    printf("Source Subscriber Number: %u \n", request_packet->source_subscriber_number);
    printf("End Packet Id: 0x%x \n", request_packet->end_packet_id);
}

/* Print response packet */
void printResponsePacket(struct ResponsePacket* response_packet){
    printf("Response Packet is: \n");
    printf("Start Packet Id: 0x%x \n", response_packet->start_packet_id);
    printf("Client Id: 0x%x \n", response_packet->client_id);
    printf("Accesss permission: 0x%x \n", response_packet->ok_not_paid_exist);
    printf("Segment Idx: 0x%x \n", response_packet->segment_no);
    printf("Packet Length: 0x%x \n", response_packet->length);
    printf("Technology: 0x%x \n", response_packet->technology);
    printf("Source Subscriber Number: %u \n", response_packet->source_subscriber_number);
    printf("End Packet Id: 0x%x \n", response_packet->end_packet_id);
}


int main(int argc, char *argv[]){
    int sockt, len, n;
    struct sockaddr_in server, from;
    struct hostent *hp;
    FILE *fp;
    
    if(argc != 3){
        printf("Not connected to host please try again \n");
        exit(0);
    }

    /* Getting file descriptor of input file which is used for getting the payload */   
    fp = fopen("input.txt","r");
    if(fp == NULL)
    {
        printf("Input file for payload does not exist \n");
    }

    /* Creating socket file descriptor 
       Type is set to SOCK_DGRM because we are providing UDP service (connectionless)*/       
    sockt = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockt < 0){
        printf("Socket not created, exiting... \n");
        exit(0);
    }

    server.sin_family = AF_INET;
    hp = gethostbyname(argv[1]);

    if(hp == 0){
        printf("Error with hostname, exiting... \n");
        exit(0);
    }

    /* Socket Timeout set is equal to TIMEOUT(3) seconds */
    struct timeval timeout;      
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;  
    setsockopt (sockt, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        
    bcopy((char*) hp->h_addr, (char*)&server.sin_addr, hp->h_length);
    server.sin_port = htons(atoi(argv[2]));
    len = sizeof(struct sockaddr_in);

    /* creating new packet */
    struct RequestPacket request_packet;
    struct RequestPacket prev_request_packet;
    
    char curr_payload[255];
    uint8_t segment_idx = 0;
    int retry_counter = 0;
    const char space[2] = " ";
    memset(curr_payload,'\0',sizeof(curr_payload));

    while((fgets(curr_payload, sizeof(curr_payload), fp) != NULL)) {
        
        /* Resetting request packet value to zero for sending again*/ 
        bzero(&request_packet, sizeof(request_packet));
        
        /* Create new packet if retransmission is not needed */
        if(retry_counter == 0){
            createPkt(&request_packet, &segment_idx);

            /* Extracting subscriber number */
            char* ptr = strtok(curr_payload, space);
            request_packet.length += strlen(ptr);
            request_packet.source_subscriber_number = atoi(ptr);
            
            /* Extracting Technology */
            ptr = strtok(NULL, space);    
            request_packet.length += strlen(ptr);
            request_packet.technology = atoi(ptr); 
        }
        else{
            request_packet = prev_request_packet;
        }
        
        memset(curr_payload,'\0',sizeof(curr_payload));

        /* Send packet to server */
        n = sendto(sockt, &request_packet, sizeof(request_packet), 0, &server, len);
        if(n < 0){
            printf("Error in sending, exiting...");
            exit(0);
        }

        /* receive packet response */
        struct ResponsePacket response_packet;
        n = recvfrom(sockt, &response_packet, sizeof(response_packet), 0, &from, &len);
        if(n <= 0){
            retry_counter++;
            if(retry_counter > 3){
                printf("Server does not respond \n");
                exit(0);
            }
            printf("Still waiting , resending again %d time \n", (retry_counter));
            prev_request_packet = request_packet;
        }
        else{
            if(response_packet.ok_not_paid_exist == NOT_PAID){
                printf("This number is not paid \n");
                
            }else if(response_packet.ok_not_paid_exist == NOT_EXIST){
                printf("This number does not exist\n");
            }else{
                printf("This number is granted access\n");
            }
            printResponsePacket(&response_packet);
            printf("\n");
            retry_counter = 0;
        }
    }
    fclose(fp);
}