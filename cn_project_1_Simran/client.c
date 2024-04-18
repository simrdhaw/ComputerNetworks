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

/* Timeout for receiving response */
#define TIMEOUT                        (3)

/* Protocol Primitives */
#define START_PACKET                   (0xFFFF)
#define END_PACKET                     (0xFFFF)
#define MAX_CLIENT_ID                  (0xFF)
#define MAX_LENGTH_PAYLOAD             (0xFF)

/* Supported Packet Types */
#define DATA_TYPE                      (0xFFF1)
#define ACK_TYPE                       (0xFFF2)
#define REJECT_TYPE                    (0xFFF3) 

/* Reject Subcodes */
#define OUT_OF_SEQUENCE_REJECT         (0xFFF4)
#define LENGTH_MISMATCH_REJECT         (0xFFF5)
#define END_OF_PACKET_MISSING_REJECT   (0xFFF6)
#define DUPLICATE_PACKET_REJECT        (0xFFF7)



uint8_t client_idx = 0x01 ; 

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

/* Create Packet sent by client */
void createPkt(struct DataPacket* data_packet, uint8_t* segment_idx){
    data_packet->start_packet_id = START_PACKET;
    data_packet->client_id = client_idx;
    data_packet->data = DATA_TYPE;
    data_packet->segment_no = *segment_idx;
    *segment_idx = (*segment_idx + 1) % 0xFF;
    data_packet->end_packet_id = END_PACKET;
}

/* Print packet sent by client */
void print(struct DataPacket* dataPacket) {
	printf("Received Data Packet is:\n");
	printf("Start of Packet ID: 0x%x\n",dataPacket->start_packet_id); 
	printf("Client ID: 0x%x\n",dataPacket->client_id); 
	printf("Data: 0x%x\n",dataPacket->data); 
	printf("Segment No.: 0x%x\n",dataPacket->segment_no);
	printf("Length: 0x%x\n",dataPacket->length);
	printf("Payload: %s\n",dataPacket->payload);
	printf("End of Packet ID: 0x%x\n",dataPacket->end_packet_id);
}

/* Print the packet response for rejected packet by server*/
void printRejectPkt( struct RejectPacket* reject_packet){
    printf("Received Reject Packet is:\n");
    printf("Start of Packet ID: 0x%x\n", reject_packet->start_packet_id);
    printf("client_id: 0x%x\n",reject_packet->client_id);
    printf("reject: 0x%x\n",reject_packet->reject);
    printf("reject_sub_code: 0x%x\n",reject_packet->reject_sub_code);
    printf("segment_no: 0x%x\n",reject_packet->segment_no);
    printf("End of Packet ID: 0x%x\n",reject_packet->end_packet_id);
}

/* Print the Ack response for correct packet */
void printAckPkt( struct AckPacket* ack_packet){
    printf("Received Ack Packet is:\n");
    printf("Start of Packet ID: 0x%x\n", ack_packet->start_packet_id);
    printf("client_id: 0x%x\n",ack_packet->client_id);
    printf("ack: 0x%x\n",ack_packet->ack);
    printf("segment_no: 0x%x\n",ack_packet->segment_no);
    printf("End of Packet ID: 0x%x\n",ack_packet->end_packet_id);
}
   
int main(int argc, char *argv[]){
    int sockt, len, n;
    struct sockaddr_in server, from;
    struct hostent *hp;
    FILE *fp;

    /* Input should have 4 arguments 
       file to be run, host, port_number, mode_error */
    if(argc != 4){
        printf("Not connected to host please try again \n");
        exit(0);
    }

    /*mode 0 means client is working without issue and sending correct packets
      mode 1 means client is sending error packet*/
    int mode_error = atoi(argv[3]);
    printf("Input parameter, mode_error : %d \n",  mode_error);
    
    /* Getting file descriptor of input file to get packet payload */
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

    //Creating new packet
    struct DataPacket data_packet;

    //Creating prev packet to handle retransmission scenarios if response is not received
    struct DataPacket prev_data_packet;

    /* Buffer for storing payload, maximum paylaod size can be of 255 bytes */
    char curr_payload[255];
    memset(curr_payload, '\0',sizeof(curr_payload));
    uint8_t segment_idx = 0;
    uint8_t retry_counter = 0;

    while((fgets(curr_payload, sizeof(curr_payload), fp) != NULL)){

        /* Resetting data packet value to zero for sending again*/ 
        bzero(&data_packet, sizeof(data_packet));
        
        /* Create new packet if no retransmission required */
        if(retry_counter == 0){
            createPkt(&data_packet, &segment_idx);
            strncpy(data_packet.payload, curr_payload, strlen(curr_payload)-1);
            data_packet.length = strlen(data_packet.payload);
        }
        else{
             data_packet = prev_data_packet;
        }
       
        memset(curr_payload, '\0',sizeof(curr_payload));

        /* mode_error = 1, for updating the packets with segment id: 1 to 4 with errors */   
        if(mode_error){
            if(data_packet.segment_no == 1){
                data_packet.segment_no = 10;
            }else if(data_packet.segment_no == 2){
                data_packet.segment_no = 1;
                data_packet.length = 0;
            }else if(data_packet.segment_no == 3){
                data_packet.segment_no = 1;
                data_packet.end_packet_id = 0xFFF0;
            }else if(data_packet.segment_no == 4){
                data_packet.segment_no = 0;
            }
        }

        /* Send packet to server */
        n = sendto(sockt, &data_packet, sizeof(data_packet), 0, &server, len);
        if(n < 0){
            printf("Error in sending, exiting...");
            exit(0);
        }

        struct RejectPacket reject_packet;
        
        /* receive packet response */
        n = recvfrom(sockt, &reject_packet, sizeof(reject_packet), 0, &from, &len);
        if(n <= 0){
            retry_counter++;
            /* Exit if after 3 times retransmission packet is not not received */
            if(retry_counter > 3){
                printf("Server does not respond\n");
                exit(0);
            }
            printf("Still waiting , resending again %d time \n", (retry_counter));
            prev_data_packet = data_packet;
        }
        else{
            /* Checking the type of response received and printing the response packet */
            if(reject_packet.reject == REJECT_TYPE){
                printf("Got a reject packet ack. \n");
                printRejectPkt(&reject_packet);
            }else{
                printf("Got an ack. \n");
                printAckPkt(&reject_packet);
            }
            printf("\n");
            retry_counter = 0;
        }   

    }
    return 0;
}
