//Name: Khoa Quach
//EMAIL: khoaquachschool@gmail.com
//ID: 105123806

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netdb.h> 

// =====================================

#define RTO 50000 /* timeout in microseconds */ // update used to be 500000
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */
#define FIN_WAIT 2 /* seconds to wait after receiving FIN*/

// KHOA: 
#define DATA_WAIT 3 /* seconds wait after receiving packet data*/


// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

double setFinTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) FIN_WAIT;
}

// KHOA: help with program exiting, make sure data can exit after receiving it
double setDataTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double)DATA_WAIT;
}
// Use for retransmission trigger
int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 4) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    struct in_addr servIP;
    if (inet_aton(argv[1], &servIP) == 0) {
        struct hostent* host_entry; 
        host_entry = gethostbyname(argv[1]); 
        if (host_entry == NULL) {
            perror("ERROR: IP address not in standard dot notation\n");
            exit(1);
        }
        servIP = *((struct in_addr*) host_entry->h_addr_list[0]);
    }

    unsigned int servPort = atoi(argv[2]);

    FILE* fp = fopen(argv[3], "r");
    if (fp == NULL) {
        perror("ERROR: File not found\n");
        exit(1);
    }

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = servIP;
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    int servaddrlen = sizeof(servaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================
    // Establish Connection: This procedure is provided to you directly and is
    // already working.
    // Note: The third step (ACK) in three way handshake is sent along with the
    // first piece of along file data thus is further below

    struct packet synpkt, synackpkt;

    unsigned short seqNum = rand() % MAX_SEQN;
    buildPkt(&synpkt, seqNum, 0, 1, 0, 0, 0, 0, NULL);

    printSend(&synpkt, 0);
    sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    double timer = setTimer();
    int n;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            else if (isTimeout(timer)) {
                printTimeout(&synpkt);
                printSend(&synpkt, 1);
                sendto(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
        }

        printRecv(&synackpkt);
        if ((synackpkt.ack || synackpkt.dupack) && synackpkt.syn && synackpkt.acknum == (seqNum + 1) % MAX_SEQN) {
            seqNum = synackpkt.acknum;
            break;
        }
    }

    // =====================================
    // FILE READING VARIABLES
    
    char buf[PAYLOAD_SIZE];
    size_t m;

    // =====================================
    // CIRCULAR BUFFER VARIABLES
    struct packet ackpkt;
    struct packet pkts[WND_SIZE];
    // KHOA: 
	struct packet save_pkts[WND_SIZE]; // send unchecked packets
	struct packet temp_pkts[WND_SIZE]; // # of unchecked packets currently sent.
    // to remove checked data packet , used alongside save_pkts
	int read_toggle = 0; // file done reading flag
	int next_seqn = 0;
	double data_flag = 0;
	int base = 0; // seq number for next phase, acknum > base = culm server seq nuum
	// server ackn < base = duplicate packet -> discard
    // ====The seq # of the first returned by the server includs file length, which will be >  base
    // base will save the ack value of the last server confirmation pkt
    // i.e saves ack value of last confirmed pkt by the server
    // pkt 0, wait till ack0 received, then update base
    // Remember, recv ack # is = to out seq #
    int curr_notackedpkts = 0; // current # packets in the window. current sliding window status
	// also the total # of unacknowledged packets being currently sent
    int i;
	int index = 0; // index server ack received for this step in the sliding window, for unchecked packet
	// the server that confirms the data packet,
    // the data packets after index are  unconfirmed data packets
    // Delete pakt and the previous pkt, only keep the data pkt after index+1
    // Purpose: index is the unconfirmed packet found,  server confirms the data packet, the ones after index are unconfirmed ones
    int limit = 1; // flag for when seqnum > MAX_SEQN
    // =====================================
    // Send First Packet (ACK containing payload)

    m = fread(buf, 1, PAYLOAD_SIZE, fp);

    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, m, buf);
	base = seqNum;//Save the packet sequence number reference for the next response
    printSend(&pkts[0], 0);
    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
	if (curr_notackedpkts < WND_SIZE)// Sliding window size is less than 10 condition is important!! // make sure unchecked pkts !> sliding window size
	{
		seqNum += m;// seqn + file data read size, seqn = expected ack seq # expected next phase
		// make sure server receives the correct seq # next phase
        seqNum %= MAX_SEQN; // get valid seqnumber
		pkts[0].seqnum = seqNum;//Save the seqnumber
		memcpy(&save_pkts[curr_notackedpkts], &pkts[0], sizeof(save_pkts[curr_notackedpkts])); // save pkt to save_pkts
        // Save unacknowledged packets that have been sent by client
		curr_notackedpkts++; // the total # of unacnowledged packets sent, increment this
		next_seqn++; // increment the window #
	}
    timer = setTimer(); // start timer
    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 1, m, buf); // build new pkt

    // =====================================
    // *** TODO: Implement the rest of reliable transfer in the client ***
    // Implement GBN for basic requirement or Selective Repeat to receive bonus

    // Note: the following code is not the complete logic. It only sends a
    //       single data packet, and then tears down the connection without
    //       handling data loss.
    //       Only for demo purpose. DO NOT USE IT in your final submission
    data_flag = setDataTimer(); //data timer
    while (1) {
        n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);
		if (n > 0)//If the packet is received
		{
			printRecv(&ackpkt);	// RECV
			timer = setTimer(); // reset pkt retrans timmer
			data_flag = setDataTimer(); //reset packet data timer
            // both timeout retransmission and exit time via DATA WAIT updated when sending and receiving stuff
			// If curr seq num > expected seq num or pkt size > MAX
            // sent sq # and received ack # , paylod * 3 =1536
            // recall we take modulus of MAX_SEQN so the logic works out :) 
            // If this is T, mark the data packet received, remove it from the window
			if (ackpkt.acknum >= base || ackpkt.acknum < PAYLOAD_SIZE * 3) // not sure about this condition, might fail some edge cases
			{
				//Find if the packet is in the curr sliding window status
				for (i=0; i < curr_notackedpkts; i++)
				{
					//data packet in the sliding window  acknum == sedqnum for saved pkts
					if (ackpkt.acknum == save_pkts[i].seqnum)
					{
						index = i; ///index is saved, i.e confirmed packet index
						break;
					}
				}
				// packet is not within the sliding window, discard 
				if (curr_notackedpkts == i)
				{
					continue;	
				}
				memcpy(temp_pkts, save_pkts, sizeof(save_pkts));//Save the sliding window data to a temp array, so we can delete the marked packets later
				index++;// Unchecked packet index increment as initial point to start algorithm
				curr_notackedpkts -= index;//decrement since we take it out after it is sent and confirmed
				//decrement next sliding window sequence number that needs to be received
				next_seqn -= index;
                // server confirms the received , use seqn and the indexto delete the confirmed data packet.
				//server's ack package is the last one needed to reset the sliding window data
                // since all unchecked pkts are confirmed in the context of this scenario
				if (curr_notackedpkts == 1)
				{
					memset(save_pkts, 0, sizeof(save_pkts));
					curr_notackedpkts = 0;	
					continue;	
				}
				//Delete the confirmed pkts  and push unconfirmed them ones forward
				for (i=0; i < curr_notackedpkts; i++)
				{
					memcpy(&save_pkts[i], &temp_pkts[index], sizeof(save_pkts[i]));
					index++;
				}
				// Save the serial number of the response packet received last time
                // prevent duplicate pkts
				base = ackpkt.acknum;
			}
		}
        /*------------------------------------------------*/
		//Sliding window data packet timeout retransmission, // loop the window size
		if (isTimeout(timer)) 
		{	 
			 for (i=0; i < curr_notackedpkts; i++) // curr pkts => unacknowledged ones in my context
			 {
			 	limit = 0; // assume not exceeded the limit 
				//If the data packet is greater than 25601, 
				//seq # (after modulo) is < PAYLOAD_SIZE
                // PAYLOAD data passed once is at most this PAYLOAD c onstant size so use it as condition
				
				if (save_pkts[i].seqnum  < PAYLOAD_SIZE)
				{
					save_pkts[i].seqnum += MAX_SEQN;
					limit = 1;
				}
				//Seq # minus data length
			 	save_pkts[i].seqnum -= save_pkts[i].length; // subtract packet data size after timeout retransmission
			 	// Resend packets!!!
                printSend(&save_pkts[i], 1);
			 	sendto(sockfd, &save_pkts[i], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);	
				save_pkts[i].seqnum += save_pkts[i].length; // seq num + data size being sent is standard
				
				if (limit == 1)
				{
					//the seq number after modulo is less than PAYLOAD_SIZE,
					save_pkts[i].seqnum -= MAX_SEQN;
					limit = 0; // now set limit to 0
				}	
				
			 }

			 timer = setTimer();////Reset the retransmission packet timer
			// If the packet receiving is complete, the packet data timer will not be updated
			 if (curr_notackedpkts > 0) // if it is 0, sliding window is 0
			 	data_flag = setDataTimer();//Reset packet data  timer
		}

		//Data packets  transmission completed 
		if (isTimeout(data_flag))
		{
			break;	
		}	

		//If the file data has not been read and the current sliding window size is less than 10
		if (read_toggle == 0 && next_seqn < WND_SIZE)
		{
			m = fread(buf, 1, PAYLOAD_SIZE, fp);/* read from file */
			if (m <= 0)
			{
				read_toggle = 1;	//Data read completion flag
				continue;
			}				

			seqNum %= MAX_SEQN;//The seq number is modulo so we don't get higher than the max allowed
			next_seqn++;//plus one
		    buildPkt(&pkts[0], seqNum, (synackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 0, 0, m, buf);
		    printSend(&pkts[0], 0);
		    sendto(sockfd, &pkts[0], PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
			timer = setTimer();//Reset the retransmission packet timer
			data_flag = setDataTimer();//Reset packet timer
            // save the sent but unchecked packets
			if (curr_notackedpkts < WND_SIZE)//If the sliding window is less than 10
			{
				seqNum += m;//The serial number plus the size of the file data read
				seqNum %= MAX_SEQN;//Take the modulo MAX_SEQN to avoid data packets larger than MAX_SEQN
				if (seqNum < PAYLOAD_SIZE)
					limit = 1;//flag set to 1 since seq # exceeded MAX_SEQ size. 
				pkts[0].seqnum = seqNum;//Save the seq num after receiving signal
				pkts[0].length = m;//Save the size of the data being read
				memcpy(&save_pkts[curr_notackedpkts], &pkts[0], sizeof(save_pkts[curr_notackedpkts]));
				curr_notackedpkts++;//slide forward in the window
			}
		}
    }

    // *** End of your client implementation ***
    fclose(fp);

    // =====================================
    // Connection Teardown: This procedure is provided to you directly and is
    // already working.

    struct packet finpkt, recvpkt;
    buildPkt(&finpkt, ackpkt.acknum, 0, 0, 1, 0, 0, 0, NULL);
    buildPkt(&ackpkt, (ackpkt.acknum + 1) % MAX_SEQN, (ackpkt.seqnum + 1) % MAX_SEQN, 0, 0, 1, 0, 0, NULL);

    printSend(&finpkt, 0);
    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
    timer = setTimer();
    int timerOn = 1;

    double finTimer;
    int finTimerOn = 0;

    while (1) {
        while (1) {
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &servaddrlen);

            if (n > 0)
                break;
            if (timerOn && isTimeout(timer)) {
                printTimeout(&finpkt);
                printSend(&finpkt, 1);
                if (finTimerOn)
                    timerOn = 0;
                else
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
                timer = setTimer();
            }
            if (finTimerOn && isTimeout(finTimer)) {
                close(sockfd);
                if (! timerOn)
                    exit(0);
            }
        }
        printRecv(&recvpkt);
        if ((recvpkt.ack || recvpkt.dupack) && recvpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN) {
            timerOn = 0;
        }
        else if (recvpkt.fin && (recvpkt.seqnum + 1) % MAX_SEQN == ackpkt.acknum) {
            printSend(&ackpkt, 0);
            sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &servaddr, servaddrlen);
            finTimer = setFinTimer();
            finTimerOn = 1;
            buildPkt(&ackpkt, ackpkt.seqnum, ackpkt.acknum, 0, 0, 0, 1, 0, NULL);
        }
    }
}
