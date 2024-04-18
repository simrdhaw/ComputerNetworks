/**
  * Author : Simran Dhawan 
  * Student Id: 7700011429 
  * Created: 02/26/2023
  * File name: server.c
**/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* Protocol Primitives */
#define START_PACKET                   (0xFFFF)
#define END_PACKET                     (0xFFFF)
#define MAX_CLIENT_ID                  (0xFF)
#define MAX_LENGTH                     (0xFF)

/* Supported Packet Types */
#define DATA_TYPE                      (0xFFF1)
#define ACK_TYPE                       (0xFFF2)
#define REJECT_TYPE                    (0xFFF3) 

/* Reject Subcodes */
#define OUT_OF_SEQUENCE_REJECT         (0xFFF4)
#define LENGTH_MISMATCH_REJECT         (0xFFF5)
#define END_OF_PACKET_MISSING_REJECT   (0xFFF6)
#define DUPLICATE_PACKET_REJECT        (0xFFF7)

uint8_t segment_idx = 0;

/* Data Packet Format */
struct DataPacket{
    uint16_t start_packet_id;
    uint8_t client_id;
    uint16_t data;
    uint8_t segment_no;
    uint8_t length;
    uint8_t payload[255];
    uint16_t end_packet_id;
};

/* Ack Packet Format */
struct AckPacket{
    uint16_t start_packet_id;
    uint8_t client_id;
    uint16_t ack;
    uint8_t segment_no;
    uint16_t end_packet_id;
};

/* Reject Packet Format */
struct RejectPacket{
    uint16_t start_packet_id;
    uint8_t client_id;
    uint16_t reject;
    uint16_t reject_sub_code;
    uint8_t segment_no;
    uint16_t end_packet_id;
};

/* System error with message and exiting */
void error(char *msg){
    perror(msg);
    exit(0);
}

/* Prints the data packet, which is sent by client */
void print(struct DataPacket* dataPacket) {
	printf("Received Packet is:\n");
	printf("Start of Packet ID: 0x%x\n",dataPacket->start_packet_id); 
	printf("Client ID: 0x%x\n",dataPacket->client_id); 
	printf("Data: 0x%x\n",dataPacket->data); 
	printf("Segment No.: 0x%x\n",dataPacket->segment_no);
	printf("Length: 0x%x\n",dataPacket->length);
	printf("Payload: %s\n",dataPacket->payload);
	printf("End of Packet ID: 0x%x\n",dataPacket->end_packet_id);
}

/* Creates server response for reject packet */
void createRejectPkt(struct RejectPacket* reject_pkt, uint8_t client_idx, uint16_t reject_code, uint8_t segment_idx){
    reject_pkt->start_packet_id = START_PACKET;
    reject_pkt->client_id = client_idx;
    reject_pkt->reject = REJECT_TYPE;
    reject_pkt->reject_sub_code = reject_code;
    reject_pkt->segment_no = segment_idx;
    reject_pkt->end_packet_id = END_PACKET;
}

/* create server response of Ack for correct packet received*/
void createAckPkt(struct AckPacket* ack_pkt, uint8_t client_idx, uint8_t segment_idx){
    ack_pkt->start_packet_id = START_PACKET;
    ack_pkt->client_id = client_idx;
    ack_pkt->ack = ACK_TYPE;
    ack_pkt->segment_no = segment_idx;
    ack_pkt->end_packet_id = END_PACKET;
}

/* packet sent by client requires validation at server before processing*/
void packetValidation(struct DataPacket* data_packet, uint8_t* seg_number, uint16_t* reject_id){
    if(data_packet->segment_no == (*seg_number - 1)){
        
        *reject_id = DUPLICATE_PACKET_REJECT;
    }
    else if(data_packet->segment_no != (*seg_number)){
        *reject_id = OUT_OF_SEQUENCE_REJECT;
    }
    else if(data_packet->length != strlen(data_packet->payload)){
        *reject_id = LENGTH_MISMATCH_REJECT;
    }
    else if(data_packet->end_packet_id != 0xFFFF){
        *reject_id = END_OF_PACKET_MISSING_REJECT;
    }
    else{
        (*seg_number) = ((*seg_number)+1)%255;
    }
}

int main(int argc, char *argv[]){
    int sockt, len, fromlen, n;
    struct sockaddr_in server;
    struct sockaddr_in from;
    char buffer[1024];
    struct RejectPacket reject_packet;
    struct AckPacket ack_packet;
    
    /* Input should have 3 arguments 
       file to be run, port_number, mode_error*/
    if(argc < 3){
        fprintf(stderr, "ERROR, no port or mode provided . Please try again \n");
        exit(0);
    }

    /* Creating socket file descriptor 
       Type is set to SOCK_DGRM because we are providing UDP service (connectionless)*/
    sockt = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockt < 0){
        error("Socket opening is not successful");
    }

    len = sizeof(server);

    /* Resetting server information to zero and then filling new information for the run */
    bzero(&server,len);
    server.sin_family =  AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

    /* Binding the socket created with server address */
    if(bind(sockt, (struct sockaddr*)&server, len) < 0){
        error("Binding not successful");
    }
    
    fromlen = sizeof(struct sockaddr_in);

    /*mode 0 means server is working without issue and sending acknoledgement after receiving the packet
      mode 1 means server is not sending acknowledgement to client even after receiving the packet*/
    int mode_error = atoi(argv[2]);
    printf("Input parameter, mode_error : %d \n", mode_error );

    uint8_t segment_number = 0;
    
    /* Server listening the messages for infinite time until exiting because of error */
    while(1){

        printf("Listening the messages \n");

        struct DataPacket data_packet;
        
        /* recieve by listening on socket */
        n = recvfrom(sockt, &data_packet, 1024, 0, (struct sockaddr *)&from, &fromlen);
        if(n < 0){
            error("recvfrom error");
        }

        print(&data_packet);
        printf("\n");
        uint16_t reject_id = 0;

        /* Packet validation before creating the response */    
        packetValidation(&data_packet, &segment_number, &reject_id);
        
        /* On packet validation, reject id is updated because of faulty packet*/
        if(reject_id != 0){

            /* create rejection packet */
            createRejectPkt(&reject_packet, data_packet.client_id, reject_id , data_packet.segment_no );

            /* Handling send or not send of response depending on mode*/
            if(mode_error == 0){
                n = sendto(sockt, &reject_packet, sizeof(struct RejectPacket), 0, (struct sockaddr *)&from, fromlen);
                if(n < 0){
                    error("sendto error");
                }
            }
        }
        else{
            /* create acknowledgement packet */
            createAckPkt(&ack_packet, data_packet.client_id, data_packet.segment_no );
            
            /* Handling send or not send of response depending on mode*/
            if(mode_error == 0){
                n = sendto(sockt, &ack_packet, sizeof(struct RejectPacket), 0, (struct sockaddr *)&from, fromlen);
                if(n < 0){
                    error("sendto error");
                }
            }
        }   
    }
    close(sockt);
    return 0;
}
