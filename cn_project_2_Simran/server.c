/**
  * Author : Simran Dhawan 
  * Student Id: 7700011429 
  * Created: 02/26/2023
  * File name: server.c
**/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

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

/* Maximum size of Database supported*/
#define MAX_DATABASE                   (100)

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

/* Subscriber Info */
struct ValidSubscriber{
    uint32_t source_subscriber_number;
    uint8_t technology;
    uint8_t paid;
};

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

/* Create Response packet */
void createResponsePkt(struct ResponsePacket* response_packet, int client_idx, int status_code, int segment_idx, int technology, uint32_t source_subscriber_number, uint8_t length){
    response_packet->start_packet_id = START_PACKET;
    response_packet->client_id = client_idx;
    response_packet->ok_not_paid_exist = status_code;
    response_packet->segment_no = segment_idx;
    response_packet->length = length;
    response_packet->technology = technology;
    response_packet->source_subscriber_number = source_subscriber_number;
    response_packet->end_packet_id = END_PACKET;
}

/* Print Response packet*/
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

/* Verificiation of request subscriber number and setting the status if it is granted access or not paid */
void requestVerification(struct RequestPacket* request_packet, uint16_t* status_code, struct ValidSubscriber database[] ){
    *status_code = NOT_EXIST;
    
    for(uint8_t num = 0; num < MAX_DATABASE; num++){
        if((database[num].source_subscriber_number == request_packet->source_subscriber_number) && (database[num].technology == request_packet->technology)){
            if(database[num].paid){
                *status_code = ACCESS_OK;
            }else{
                *status_code = NOT_PAID;
            }
            return;
        }
    }
    return;
}


int main(int argc, char *argv[]){

    /* File descriptor for accessing the database containing information of subcriber number and technology and payment status */
    FILE *fp;
    fp = fopen("Verification_Database.txt","r");
    if(fp == NULL)
    {
        printf("Input file for payload does not exist \n");
    }

    char curr_line[40]; 
    struct ValidSubscriber database[100];
    int num = 0;
    const char space[2] = " ";

    /* Creating database of subscribers */
    while(fgets(curr_line, sizeof(curr_line), fp) != NULL){
        char* ptr = NULL;
        /* Extracting Subscriber Number from Database*/
        ptr = strtok(curr_line, space);       
        database[num].source_subscriber_number = atol(ptr);

        /* Extracting Technology from Database*/
        ptr = strtok(NULL, space);
        database[num].technology = atoi(ptr);

        /* Extracting Payment status from Database*/
        ptr = strtok(NULL,space);
        database[num].paid = atoi(ptr);

        num++;
    }
    
    fclose(fp);


    int sockt, len, fromlen, n;
    struct sockaddr_in server;
    struct sockaddr_in from;
    struct RequestPacket request_packet;
    struct ResponsePacket response_packet;
    
    /* Input should have 3 arguments 
       file to be run, port_number, mode_error*/
    if(argc < 3){
        fprintf(stderr, "ERROR, no port or mode provided . Please try again \n");
        exit(0);
    }

    /*mode 0 means client is working without issue and sending correct packets
      mode 1 means client is sending error packet*/
    int mode_error = atoi(argv[2]);
    printf("Input parameter, mode_error : %d \n",  mode_error);

    /* Creating socket file descriptor 
       Type is set to SOCK_DGRM because we are providing UDP service (connectionless)*/
    sockt = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockt < 0){
        error("Socket opening is not successful");
    }

    /* Resetting server information to zero and then filling new information for the run */
    len = sizeof(server);
    bzero(&server,len);
    server.sin_family =  AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

    /* Binding the socket created with server address */
    if(bind(sockt, (struct sockaddr*)&server, len) < 0){
        error("Binding not successful");
    }

    fromlen = sizeof(struct sockaddr_in);

    /* Server listening the messages for infinite time until exiting because of error */
    while(1){
        n = recvfrom(sockt, &request_packet, sizeof(request_packet), 0, (struct sockaddr *)&from, &fromlen);
        if(n < 0){
            error("recvfrom error");
        }

        printRequestPacket(&request_packet);
        printf("\n");
        
        uint16_t status_code = 0 ;

        /* Verifying the request with verfication database and setting the status code*/
        requestVerification(&request_packet, &status_code, database);
        
        /* Sending response only when timeout test case with mode_error = 1 is not enabled*/
        if(mode_error == 0){
            createResponsePkt(&response_packet, request_packet.client_id, status_code, request_packet.segment_no, request_packet.technology, request_packet.source_subscriber_number, request_packet.length);      
            n = sendto(sockt, &response_packet, sizeof(struct ResponsePacket), 0, (struct sockaddr *)&from, fromlen);
            if(n < 0){
                error("sendto error");
            }
        }
    }
}