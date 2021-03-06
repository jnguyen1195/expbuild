#include<stdio.h>		
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>		/* To set non blocking on socket  */
#include<sys/socket.h>		/* Generic socket calls */
#include<sys/ioctl.h>
#include<time.h>
#include<sys/types.h>
#include<signal.h>
#include<linux/if_packet.h>
#include<linux/if_ether.h>
#include<linux/if_arp.h>
#include "headers.h"

//Defined in dhtest.c
extern int sock_packet;
extern struct sockaddr_ll ll;
extern int iface;
extern u_int16_t vlan;
extern u_int8_t l3_tos;

extern u_int16_t l2_hdr_size;
extern u_int16_t l3_hdr_size;
extern u_int16_t l4_hdr_size;
extern u_int16_t dhcp_hdr_size;

extern u_char dhcp_packet_disc[1514];
extern u_char dhcp_packet_offer[1514];
extern u_char dhcp_packet_request[1514];
extern u_char dhcp_packet_ack[1514];
extern u_char dhcp_packet_release[1514];

extern u_int8_t dhopt_buff[500];
extern u_int32_t dhopt_size;
extern u_int32_t dhcp_xid;
extern u_int32_t bcast_flag;
extern u_int8_t timeout;
extern u_int8_t vci_buff[256];

extern struct ethernet_hdr *eth_hg;
extern struct vlan_hdr *vlan_hg; 
extern struct iphdr *iph_g;
extern struct udphdr *uh_g;
extern struct dhcpv4_hdr *dhcph_g;
extern u_int8_t *dhopt_pointer_g;

struct arp_hdr *arp_hg;
struct icmp_hdr *icmp_hg;

extern u_char dhmac[ETHER_ADDR_LEN];
extern u_char dmac[ETHER_ADDR_LEN];

extern char dhmac_fname[20];
extern char iface_name[30];
extern u_int32_t server_id, option50_ip;
extern u_int8_t dhcp_release_flag;

extern u_int32_t ip_address;
extern ip_listen_flag;
extern u_char arp_icmp_packet[1514];
extern u_char arp_icmp_reply[1514];
extern u_int16_t icmp_len;
extern struct timeval tval_listen;
extern u_int32_t listen_timeout;

/*
 * Opens PF_PACKET socket and return error if socket
 * opens fails
 */
 
int open_socket()
{
    int sock_new, non_block, tmp;
    sock_packet = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sock_packet < 0) {
	perror("--Error on creating the socket--");
	return SOCKET_ERR;
    } 
    /* Set link layer parameters */
    ll.sll_family = AF_PACKET;
    ll.sll_protocol = htons(ETH_P_ALL);
    ll.sll_ifindex = iface; 
    ll.sll_hatype = ARPHRD_ETHER;
    ll.sll_pkttype = PACKET_OTHERHOST;
    ll.sll_halen = 6;
    
    bind(sock_packet, (struct sockaddr *)&ll, sizeof(struct sockaddr_ll));
    return 0;
}

/*
 * Closes PF_PACKET socket
 */
int close_socket()
{
    close(sock_packet);
    return 0;
}

/*
 * Sets the promiscous mode on the interface
 */
int set_promisc() 
{
    int status;
    struct ifreq ifr;
    if(!strlen(iface_name)) {
	strcpy(iface_name, "eth0");
    }
    strcpy(ifr.ifr_name, iface_name);
    ifr.ifr_flags = (IFF_PROMISC | IFF_UP);
    status = ioctl(sock_packet, SIOCSIFFLAGS, &ifr);
    if(status < 0) {
	perror("Error on setting promisc");
	exit(1);
    }
}

int clear_promisc() 
{
    int status;
    struct ifreq ifr;
    strcpy(ifr.ifr_name, iface_name);
    ifr.ifr_flags = IFF_UP;
    status = ioctl(sock_packet, SIOCSIFFLAGS, &ifr);
    if(status < 0) {
	perror("Error on disabling promisc");
	exit(1);
    }
    return 0;
}

/*
 * Sends DHCP packet on the socket. Packet type 
 * is passed as argument. Extended to send ARP and ICMP packets
 */
int send_packet(int pkt_type)
{
    int ret;
    if(pkt_type == DHCP_MSGDISCOVER) {
	ret = sendto(sock_packet,\
		dhcp_packet_disc,\
		(l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size + dhopt_size),\
		0,\
		(struct sockaddr *) &ll,\
		sizeof(ll));
    } else if(pkt_type == DHCP_MSGREQUEST) {
	ret = sendto(sock_packet,\
		dhcp_packet_request,\
		(l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size + dhopt_size),\
		0,\
		(struct sockaddr *) &ll,\
		sizeof(ll));
    } else if(pkt_type == DHCP_MSGRELEASE) {
	ret = sendto(sock_packet,\
		dhcp_packet_release,\
		(l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size + dhopt_size),\
		0,\
		(struct sockaddr *) &ll,\
		sizeof(ll));
    } else if(pkt_type == ARP_SEND) {
	ret = sendto(sock_packet,\
		arp_icmp_reply,\
		60,\
		0,\
		(struct sockaddr *) &ll,\
		sizeof(ll));
    } else if(pkt_type == ICMP_SEND) {
	ret = sendto(sock_packet,\
		arp_icmp_reply,\
		(l2_hdr_size + l3_hdr_size + ICMP_H + icmp_len),\
		0,\
		(struct sockaddr *) &ll,\
		sizeof(ll));
    }
    
    if(ret < 0) {
	perror("Paket send failure");
	close(sock_packet);
	exit(1);
	return PACK_SEND_ERR;
    } else {
	if(pkt_type == DHCP_MSGDISCOVER) {
	    fprintf(stdout, "DHCP discover sent\t - ");
	    fprintf(stdout, "Client MAC : %02x:%02x:%02x:%02x:%02x:%02x\n", \
	    dhmac[0], dhmac[1], dhmac[2], dhmac[3], dhmac[4], dhmac[5]);
	} else if (pkt_type == DHCP_MSGREQUEST) {
	    fprintf(stdout, "DHCP request sent\t - ");
	    fprintf(stdout, "Client MAC : %02x:%02x:%02x:%02x:%02x:%02x\n", \
	    dhmac[0], dhmac[1], dhmac[2], dhmac[3], dhmac[4], dhmac[5]); 
	} else if (pkt_type == DHCP_MSGRELEASE) { 
	    fprintf(stdout, "DHCP release sent\t - ");
	    fprintf(stdout, "Client MAC : %02x:%02x:%02x:%02x:%02x:%02x\n", \
	    dhmac[0], dhmac[1], dhmac[2], dhmac[3], dhmac[4], dhmac[5]); 
	}
    }
    return 0;
}

/*
 * Receives DHCP packet. Packet type is passed as argument
 * Extended to recv ARP and ICMP packets
 */
int recv_packet(int pkt_type) 

{
    int ret, sock_len, retval, chk_pkt_state, tmp = 0;
    fd_set read_fd;
    struct timeval tval;
    tval.tv_sec = 5; 
    tval.tv_usec = 0;

    if(pkt_type == DHCP_MSGOFFER) {
	while(tval.tv_sec != 0) {
	    FD_ZERO(&read_fd);
	    FD_SET(sock_packet, &read_fd);
	    retval = select(sock_packet + 1, &read_fd, NULL, NULL, &tval);
	    if(retval == 0) {
		return DHCP_DISC_RESEND;
		break;
	    } else if ( retval > 0 && FD_ISSET(sock_packet, &read_fd)) {
		bzero(dhcp_packet_offer, sizeof(dhcp_packet_offer));
		sock_len = sizeof(ll);
		ret = recvfrom(sock_packet,\
			dhcp_packet_offer,\
			sizeof(dhcp_packet_offer),\
			0,\
			(struct sockaddr *)&ll,
			&sock_len);
	    }
	    if(ret >= 60) {
		chk_pkt_state = check_packet(DHCP_MSGOFFER);
		if(chk_pkt_state == DHCP_OFFR_RCVD) {
		    return DHCP_OFFR_RCVD;
		}
	    } 
	}
	return DHCP_DISC_RESEND;
    } else if(pkt_type == DHCP_MSGACK) {
	while(tval.tv_sec != 0) {
	    FD_ZERO(&read_fd);
	    FD_SET(sock_packet, &read_fd);
	    retval = select(sock_packet + 1, &read_fd, NULL, NULL, &tval);
	    if(retval == 0) {
		return DHCP_REQ_RESEND;
		break;
	    } else if ( retval > 0 && FD_ISSET(sock_packet, &read_fd)){
		bzero(dhcp_packet_ack, sizeof(dhcp_packet_ack));
		sock_len = sizeof(ll);
		ret = recvfrom(sock_packet,\
			dhcp_packet_ack,\
			sizeof(dhcp_packet_ack),\
			0,\
			(struct sockaddr *)&ll,
			&sock_len);
	    }
	    if(ret >= 60) {
		chk_pkt_state = check_packet(DHCP_MSGACK);
		if(chk_pkt_state == DHCP_ACK_RCVD) {
		    return DHCP_ACK_RCVD;
		} else if(chk_pkt_state == DHCP_NAK_RCVD) {
		    return DHCP_NAK_RCVD;
		}
	    } 
	}
	return DHCP_REQ_RESEND;
    } else if(pkt_type == ARP_ICMP_RCV) {
	while(tval_listen.tv_sec != 0) {
	    FD_ZERO(&read_fd);
	    FD_SET(sock_packet, &read_fd);
	    retval = select(sock_packet + 1, &read_fd, NULL, NULL, &tval_listen);
	    if(retval == 0) {
		return LISTEN_TIMOUET;
		break;
	    } else if ( retval > 0 && FD_ISSET(sock_packet, &read_fd)) {
		bzero(arp_icmp_packet, sizeof(arp_icmp_packet));
		sock_len = sizeof(ll);
		ret = recvfrom(sock_packet,\
			arp_icmp_packet,\
			sizeof(arp_icmp_packet),\
			0,\
			(struct sockaddr *)&ll,
			&sock_len);
	    }
	    if(ret >= 60) {
		chk_pkt_state = check_packet(ARP_ICMP_RCV);
		if(chk_pkt_state == ARP_RCVD) {
		    return ARP_RCVD;
		    break;
		} else if(chk_pkt_state == ICMP_RCVD) {
		    return ICMP_RCVD;
		    break;
		}
	    } 
	}
	return LISTEN_TIMOUET;
    }
}

/* Debug function - Prints the buffer on HEX format */
int print_buff(u_int8_t *buff, int size)
{
    int tmp;
    fprintf(stdout, "\n---------Buffer data-------\n");
    for(tmp = 0; tmp < size; tmp++) {
    fprintf(stdout, "%02X ", buff[tmp]);
    if((tmp % 16) == 0 && tmp != 0) {
	fprintf(stdout, "\n");
    }
    }
    fprintf(stdout, "\n");
    return 0;
}

/* Reset the DHCP option buffer to zero and dhopt_size to zero */
int reset_dhopt_size()
{
    bzero(dhopt_buff, sizeof(dhopt_buff));
    dhopt_size = 0;
    return 0;
}

/*
 * Sets a random DHCP xid
 */
int set_rand_dhcp_xid()
{
    if(dhcp_xid == 0) {
	srand(time(NULL));
	dhcp_xid = rand() % 0xffffffff;
    }
    return 0;
}

/*
 * IP checksum function - Calculates the IP checksum
 */
u_int16_t ipchksum(u_int16_t *buff, int words) 
{
    unsigned int sum, i;
    sum = 0;
    for(i = 0;i < words; i++){
	sum = sum + *(buff + i);
    }
    sum = (sum >> 16) + sum;
    return (u_int16_t)~sum;
}

/*
 * ICMP checksum function - Calculates the ICMP checksum
 */
u_int16_t icmpchksum(u_int16_t *buff, int words) 
{
    unsigned int sum, i;
    unsigned int last_word = 0;
    /* Checksum enhancement for odd packets */
    if((icmp_len % 2) == 1) {
	last_word = *((u_int8_t *)buff + icmp_len + ICMP_H - 1);
	last_word = (htons(last_word) << 8);
	sum = 0;
	for(i = 0;i < words; i++){
	    sum = sum + *(buff + i);
	}
	sum = sum + last_word;
	sum = (sum >> 16) + sum;
	return (u_int16_t)~sum;
    } else {
	sum = 0;
	for(i = 0;i < words; i++){
	    sum = sum + *(buff + i);
	}
	sum = (sum >> 16) + sum;
	return (u_int16_t)~sum;
    }
}

/*
 * TCP/UDP checksum function
 */
u_int16_t l4_sum(u_int16_t *buff, int words, u_int16_t *srcaddr, u_int16_t *dstaddr, u_int16_t proto, u_int16_t len) 
{
    unsigned int sum, i, last_word = 0;
    
    /* Checksum enhancement - Support for odd byte packets */
    if((htons(len) % 2) == 1) {
	last_word = *((u_int8_t *)buff + ntohs(len) - 1);
	last_word = (htons(last_word) << 8);
	sum = 0;
	for(i = 0;i < words; i++){
	    sum = sum + *(buff + i);
	}
	sum = sum + last_word;
	sum = sum + *(srcaddr) + *(srcaddr + 1) + *(dstaddr) + *(dstaddr + 1) + proto + len;
	sum = (sum >> 16) + sum;
	return ~sum;
    } else {
	/* Original checksum function */
	sum = 0;
	for(i = 0;i < words; i++){
	    sum = sum + *(buff + i);
	}

	sum = sum + *(srcaddr) + *(srcaddr + 1) + *(dstaddr) + *(dstaddr + 1) + proto + len;
	sum = (sum >> 16) + sum;
	return ~sum;
    }
}

/*
 * Builds DHCP option53 on dhopt_buff
 */
int build_option53(int msg_type)
{
    if(msg_type == DHCP_MSGDISCOVER) {
	u_int8_t msgtype = DHCP_MESSAGETYPE;
	u_int8_t msglen = 1;
	u_int8_t msg = DHCP_MSGDISCOVER;

	memcpy(dhopt_buff, &msgtype, 1);
	strncpy((dhopt_buff + 1), &msglen, 1);
	strncpy((dhopt_buff + 2), &msg, 1);
	dhopt_size = dhopt_size + 3; 
    } else if(msg_type == DHCP_MSGREQUEST) {
	u_int8_t msgtype = DHCP_MESSAGETYPE;
	u_int8_t msglen = 1;
	u_int8_t msg = DHCP_MSGREQUEST;
	
	memcpy(dhopt_buff, &msgtype, 1);
	strncpy((dhopt_buff + 1), &msglen, 1);
	strncpy((dhopt_buff + 2), &msg, 1);
	dhopt_size = dhopt_size + 3; 
    } else if(msg_type == DHCP_MSGRELEASE) {
	u_int8_t msgtype = DHCP_MESSAGETYPE;
	u_int8_t msglen = 1;
	u_int8_t msg = DHCP_MSGRELEASE;
	
	memcpy(dhopt_buff, &msgtype, 1);
	strncpy((dhopt_buff + 1), &msglen, 1);
	strncpy((dhopt_buff + 2), &msg, 1);
	dhopt_size = dhopt_size + 3; 
    }
    return 0;
}

/*
 * Builds DHCP option50 on dhopt_buff
 */
int build_option50()
{
    u_int8_t msgtype = DHCP_REQUESTEDIP;
    u_int8_t msglen = 4;
    u_int32_t msg = option50_ip; 

    memcpy((dhopt_buff + dhopt_size), &msgtype, 1);
    memcpy((dhopt_buff + dhopt_size + 1), &msglen, 1);
    memcpy((dhopt_buff + dhopt_size + 2), &msg, 4);
    dhopt_size = dhopt_size + 6; 
    return 0;
}

/*
 * Builds DHCP option54 on dhopt_buff
 */
int build_option54()
{
    u_int8_t msgtype = DHCP_SERVIDENT;
    u_int8_t msglen = 4;
    u_int32_t msg = server_id;
    
    memcpy((dhopt_buff + dhopt_size), &msgtype, 1);
    memcpy((dhopt_buff + dhopt_size + 1), &msglen, 1);
    memcpy((dhopt_buff + dhopt_size + 2), &msg, 4);
    dhopt_size = dhopt_size + 6; 
}

/*
 * Builds DHCP option55 on dhopt_buff
 */
int build_option55() 
{
    u_int32_t msgtype = DHCP_PARAMREQUEST;
    u_int32_t msglen = 4;
    u_int8_t msg[4] = { 0 };
    msg[0] = DHCP_SUBNETMASK;
    msg[1] = DHCP_ROUTER;
    msg[2] = DHCP_DOMAINNAME;
    msg[3] = DHCP_DNS;
    /* msg[4] = DHCP_LOGSERV; */
    
    memcpy((dhopt_buff + dhopt_size), &msgtype, 1);
    memcpy((dhopt_buff + dhopt_size + 1), &msglen, 1);
    memcpy((dhopt_buff + dhopt_size + 2), msg, 4);
    dhopt_size = dhopt_size + 6; 
    return 0;
}

/*
 * Builds DHCP option60 on dhopt_buff
 */
int build_option60_vci()
{
    u_int32_t msgtype = DHCP_CLASSSID;
    u_int32_t msglen = strlen(vci_buff);
    
    memcpy((dhopt_buff + dhopt_size), &msgtype, 1);
    memcpy((dhopt_buff + dhopt_size + 1), &msglen, 1);
    memcpy((dhopt_buff + dhopt_size + 2), vci_buff, strlen(vci_buff));

    dhopt_size = dhopt_size + 2 + strlen(vci_buff);
    return 0;
}

/*
 * Builds DHCP end of option on dhopt_buff
 */
int build_optioneof()
{
    u_int8_t eof = 0xff;
    memcpy((dhopt_buff + dhopt_size), &eof, 1);
    dhopt_size = dhopt_size + 1; 
    return 0;
}

/*
 * Build DHCP packet. Packet type is passed as argument
 */
int build_dhpacket(int pkt_type)
{
   if(!dhcp_release_flag) {
       u_char dmac_tmp[ETHER_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
       memcpy(dmac, dmac_tmp, ETHER_ADDR_LEN);
   }
   if(pkt_type == DHCP_MSGDISCOVER) {
       if(vlan == 0) {
	   struct ethernet_hdr *ethhdr = (struct ethernet_hdr *)dhcp_packet_disc;
	   memcpy(ethhdr->ether_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(ethhdr->ether_shost, dhmac, ETHER_ADDR_LEN);
	   ethhdr->ether_type = htons(ETHERTYPE_IP);
       } else {
	   struct vlan_hdr *vhdr = (struct vlan_hdr *)dhcp_packet_disc;
	   memcpy(vhdr->vlan_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(vhdr->vlan_shost, dhmac, ETHER_ADDR_LEN);
	   vhdr->vlan_tpi = htons(ETHERTYPE_VLAN);
	   vhdr->vlan_priority_c_vid = htons(vlan);
	   vhdr->vlan_len = htons(ETHERTYPE_IP);
       }
       //print_buff(dhcp_packet_disc, sizeof(struct ethernet_hdr));
       
       struct iphdr *iph = (struct iphdr *)(dhcp_packet_disc + l2_hdr_size);
       iph->version = 4;
       iph->ihl = 5;
       iph->tos = l3_tos;
       iph->tot_len = htons(l3_hdr_size +  l4_hdr_size + dhcp_hdr_size + dhopt_size);  
       iph->id = 0;
       iph->frag_off = 0;
       iph->ttl = 64;
       iph->protocol = 17;
       iph->check = 0; // Filled later;
       iph->saddr = inet_addr("0.0.0.0");
       iph->daddr = inet_addr("255.255.255.255");
       iph->check = ipchksum((u_int16_t *)(dhcp_packet_disc + l2_hdr_size), iph->ihl << 1);

       struct udphdr *uh = (struct udphdr *) (dhcp_packet_disc + l2_hdr_size + l3_hdr_size);
       uh->source = htons(68);
       uh->dest = htons(67);
       u_int16_t l4_proto = 17;
       u_int16_t l4_len = (l4_hdr_size + dhcp_hdr_size + dhopt_size);
       uh->len = htons(l4_len);
       uh->check = 0; /* UDP checksum will be done after dhcp header*/

       struct dhcpv4_hdr *dhpointer = (struct dhcpv4_hdr *)(dhcp_packet_disc + l2_hdr_size + l3_hdr_size + l4_hdr_size);
       dhpointer->dhcp_opcode = DHCP_REQUEST;
       dhpointer->dhcp_htype = ARPHRD_ETHER;
       dhpointer->dhcp_hlen = ETHER_ADDR_LEN;
       dhpointer->dhcp_hopcount = 0;
       dhpointer->dhcp_xid = htonl(dhcp_xid);
       dhpointer->dhcp_secs = 0;
       dhpointer->dhcp_flags = bcast_flag;
       dhpointer->dhcp_cip = 0;
       dhpointer->dhcp_yip = 0;
       dhpointer->dhcp_sip = 0;
       dhpointer->dhcp_gip = 0;
       memcpy(dhpointer->dhcp_chaddr, dhmac, ETHER_ADDR_LEN);
       /*dhpointer->dhcp_sname 
       dhpointer->dhcp_file*/
       dhpointer->dhcp_magic = htonl(DHCP_MAGIC);

       /* DHCP option buffer is copied here to DHCP packet */
       u_char *dhopt_pointer = (u_char *)(dhcp_packet_disc + l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size);
       memcpy(dhopt_pointer, dhopt_buff, dhopt_size);

       /* UDP checksum is done here */
       uh->check = l4_sum((u_int16_t *) (dhcp_packet_disc + l2_hdr_size + l3_hdr_size), ((dhcp_hdr_size + dhopt_size + l4_hdr_size) / 2), (u_int16_t *)&iph->saddr, (u_int16_t *)&iph->daddr, htons(l4_proto), htons(l4_len)); 
   }
   if(pkt_type == DHCP_MSGREQUEST) {
       if(vlan == 0) {
	   struct ethernet_hdr *ethhdr = (struct ethernet_hdr *)dhcp_packet_request;
	   memcpy(ethhdr->ether_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(ethhdr->ether_shost, dhmac, ETHER_ADDR_LEN);
	   ethhdr->ether_type = htons(ETHERTYPE_IP);
       } else {
	   struct vlan_hdr *vhdr = (struct vlan_hdr *)dhcp_packet_request;
	   memcpy(vhdr->vlan_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(vhdr->vlan_shost, dhmac, ETHER_ADDR_LEN);
	   vhdr->vlan_tpi = htons(ETHERTYPE_VLAN);
	   vhdr->vlan_priority_c_vid = htons(vlan);
	   vhdr->vlan_len = htons(ETHERTYPE_IP);
       }
       //print_buff(dhcp_packet_request, sizeof(struct ethernet_hdr));

       struct iphdr *iph = (struct iphdr *)(dhcp_packet_request + l2_hdr_size);
       iph->version = 4;
       iph->ihl = 5;
       iph->tos = l3_tos;
       iph->tot_len = htons(l3_hdr_size +  l4_hdr_size + dhcp_hdr_size + dhopt_size);  
       iph->id = 0;
       iph->frag_off = 0;
       iph->ttl = 64;
       iph->protocol = 17;
       iph->check = 0; // Filled later;
       iph->saddr = inet_addr("0.0.0.0");
       iph->daddr = inet_addr("255.255.255.255");
       iph->check = ipchksum((u_int16_t *)(dhcp_packet_request + l2_hdr_size), iph->ihl << 1);

       struct udphdr *uh = (struct udphdr *) (dhcp_packet_request + l2_hdr_size + l3_hdr_size);
       uh->source = htons(68);
       uh->dest = htons(67);
       u_int16_t l4_proto = 17;
       u_int16_t l4_len = (l4_hdr_size + dhcp_hdr_size + dhopt_size);
       uh->len = htons(l4_len);
       uh->check = 0; /* UDP checksum will be done after building dhcp header*/

       struct dhcpv4_hdr *dhpointer = (struct dhcpv4_hdr *)(dhcp_packet_request + l2_hdr_size + l3_hdr_size + l4_hdr_size);
       dhpointer->dhcp_opcode = DHCP_REQUEST;
       dhpointer->dhcp_htype = ARPHRD_ETHER;
       dhpointer->dhcp_hlen = ETHER_ADDR_LEN;
       dhpointer->dhcp_hopcount = 0;
       dhpointer->dhcp_xid = htonl(dhcp_xid);
       dhpointer->dhcp_secs = 0;
       dhpointer->dhcp_flags = bcast_flag;
       dhpointer->dhcp_cip = 0;
       dhpointer->dhcp_yip = 0;
       dhpointer->dhcp_sip = 0;
       dhpointer->dhcp_gip = 0;
       memcpy(dhpointer->dhcp_chaddr, dhmac, ETHER_ADDR_LEN);
       /*dhpointer->dhcp_sname 
	 dhpointer->dhcp_file*/
       dhpointer->dhcp_magic = htonl(DHCP_MAGIC);

       /* DHCP option buffer is copied here to DHCP packet */
       u_char *dhopt_pointer = (u_char *)(dhcp_packet_request + l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size);
       memcpy(dhopt_pointer, dhopt_buff, dhopt_size);

       /* UDP checksum is done here */
       uh->check = l4_sum((u_int16_t *) (dhcp_packet_request + l2_hdr_size + l3_hdr_size), ((dhcp_hdr_size + dhopt_size + l4_hdr_size) / 2), (u_int16_t *)&iph->saddr, (u_int16_t *)&iph->daddr, htons(l4_proto), htons(l4_len)); 
   }
   if(pkt_type == DHCP_MSGRELEASE) {
       if(vlan == 0) {
	   struct ethernet_hdr *ethhdr = (struct ethernet_hdr *)dhcp_packet_release;
	   memcpy(ethhdr->ether_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(ethhdr->ether_shost, dhmac, ETHER_ADDR_LEN);
	   ethhdr->ether_type = htons(ETHERTYPE_IP);
       } else {
	   struct vlan_hdr *vhdr = (struct vlan_hdr *)dhcp_packet_release;
	   memcpy(vhdr->vlan_dhost, dmac, ETHER_ADDR_LEN);
	   memcpy(vhdr->vlan_shost, dhmac, ETHER_ADDR_LEN);
	   vhdr->vlan_tpi = htons(ETHERTYPE_VLAN);
	   vhdr->vlan_priority_c_vid = htons(vlan);
	   vhdr->vlan_len = htons(ETHERTYPE_IP);
       }
       //print_buff(dhcp_packet_disc, sizeof(struct ethernet_hdr));

       struct iphdr *iph = (struct iphdr *)(dhcp_packet_release + l2_hdr_size);
       iph->version = 4;
       iph->ihl = 5;
       iph->tos = l3_tos;
       iph->tot_len = htons(l3_hdr_size +  l4_hdr_size + dhcp_hdr_size + dhopt_size);  
       iph->id = 0;
       iph->frag_off = 0;
       iph->ttl = 64;
       iph->protocol = 17;
       iph->check = 0; // Filled later;
       iph->saddr = option50_ip; //inet_addr("0.0.0.0");
       iph->daddr = server_id; //inet_addr("255.255.255.255");
       iph->check = ipchksum((u_int16_t *)(dhcp_packet_release + l2_hdr_size), iph->ihl << 1);

       struct udphdr *uh = (struct udphdr *) (dhcp_packet_release + l2_hdr_size + l3_hdr_size);
       uh->source = htons(68);
       uh->dest = htons(67);
       u_int16_t l4_proto = 17;
       u_int16_t l4_len = (l4_hdr_size + dhcp_hdr_size + dhopt_size);
       uh->len = htons(l4_len);
       uh->check = 0; /* UDP checksum will be done after dhcp header*/

       struct dhcpv4_hdr *dhpointer = (struct dhcpv4_hdr *)(dhcp_packet_release + l2_hdr_size + l3_hdr_size + l4_hdr_size);
       dhpointer->dhcp_opcode = DHCP_REQUEST;
       dhpointer->dhcp_htype = ARPHRD_ETHER;
       dhpointer->dhcp_hlen = ETHER_ADDR_LEN;
       dhpointer->dhcp_hopcount = 0;
       dhpointer->dhcp_xid = htonl(dhcp_xid);
       dhpointer->dhcp_secs = 0;
       dhpointer->dhcp_flags = bcast_flag;
       dhpointer->dhcp_cip = option50_ip;
       dhpointer->dhcp_yip = 0;
       dhpointer->dhcp_sip = 0;
       dhpointer->dhcp_gip = 0;
       memcpy(dhpointer->dhcp_chaddr, dhmac, ETHER_ADDR_LEN);
       /*dhpointer->dhcp_sname 
	 dhpointer->dhcp_file*/
       dhpointer->dhcp_magic = htonl(DHCP_MAGIC);

       /* DHCP option buffer is copied here to DHCP packet */
       u_char *dhopt_pointer = (u_char *)(dhcp_packet_release + l2_hdr_size + l3_hdr_size + l4_hdr_size + dhcp_hdr_size);
       memcpy(dhopt_pointer, dhopt_buff, dhopt_size);

       /* UDP checksum is done here */
       uh->check = l4_sum((u_int16_t *) (dhcp_packet_release + l2_hdr_size + l3_hdr_size), ((dhcp_hdr_size + dhopt_size + l4_hdr_size) / 2), (u_int16_t *)&iph->saddr, (u_int16_t *)&iph->daddr, htons(l4_proto), htons(l4_len)); 
   }
}

/*
 * build packet - Builds ARP reply and ICMP reply packets
 */
int build_packet(int pkt_type)
{
    bzero(arp_icmp_reply, sizeof(arp_icmp_reply));
    if(pkt_type == ARP_SEND) {
	map_all_layer_ptr(ARP_MAP);
	if(vlan == 0) {
	    struct ethernet_hdr *ethhdr = (struct ethernet_hdr *)arp_icmp_reply;
	    memcpy(ethhdr->ether_dhost, eth_hg->ether_shost, ETHER_ADDR_LEN);
	    memcpy(ethhdr->ether_shost, dhmac, ETHER_ADDR_LEN);
	    ethhdr->ether_type = htons(ETHERTYPE_ARP);
	} else {
	    struct vlan_hdr *vhdr = (struct vlan_hdr *)arp_icmp_reply;
	    memcpy(vhdr->vlan_dhost, vlan_hg->vlan_shost, ETHER_ADDR_LEN);
	    memcpy(vhdr->vlan_shost, dhmac, ETHER_ADDR_LEN);
	    vhdr->vlan_tpi = htons(ETHERTYPE_VLAN);
	    vhdr->vlan_priority_c_vid = htons(vlan);
	    vhdr->vlan_len = htons(ETHERTYPE_ARP);
	}
	struct arp_hdr *arph = (struct arp_hdr *)(arp_icmp_reply + l2_hdr_size);
	arph->ar_hrd = htons(ARPHRD_ETHER);
	arph->ar_pro = htons(ETHERTYPE_IP);
	arph->ar_hln = ETHER_ADDR_LEN;
	arph->ar_pln = IP_ADDR_LEN;
	arph->ar_op = htons(ARPOP_REPLY);
	u_int32_t ip_addr_tmp;
	ip_addr_tmp = htonl(ip_address);
	memcpy(arph->sender_mac, dhmac, ETHER_ADDR_LEN);
	memcpy(arph->sender_ip, (u_char *)&ip_addr_tmp, ETHER_ADDR_LEN);
	memcpy(arph->target_mac, arp_hg->sender_mac, ETHER_ADDR_LEN);
	memcpy(arph->target_ip, arp_hg->sender_ip, IP_ADDR_LEN);
    } else if(ICMP_SEND) {
	map_all_layer_ptr(ICMP_MAP);
	if(vlan == 0) {
	    struct ethernet_hdr *ethhdr = (struct ethernet_hdr *)arp_icmp_reply;
	    memcpy(ethhdr->ether_dhost, eth_hg->ether_shost, ETHER_ADDR_LEN);
	    memcpy(ethhdr->ether_shost, dhmac, ETHER_ADDR_LEN);
	    ethhdr->ether_type = htons(ETHERTYPE_IP);
	} else {
	    struct vlan_hdr *vhdr = (struct vlan_hdr *)arp_icmp_reply;
	    memcpy(vhdr->vlan_dhost, vlan_hg->vlan_shost, ETHER_ADDR_LEN);
	    memcpy(vhdr->vlan_shost, dhmac, ETHER_ADDR_LEN);
	    vhdr->vlan_tpi = htons(ETHERTYPE_VLAN);
	    vhdr->vlan_priority_c_vid = htons(vlan);
	    vhdr->vlan_len = htons(ETHERTYPE_IP);
	}
	//print_buff(dhcp_packet_request, sizeof(struct ethernet_hdr));

	struct iphdr *iph = (struct iphdr *)(arp_icmp_reply + l2_hdr_size);
	iph->version = 4;
	iph->ihl = 5;
	iph->tos = l3_tos;
	iph->tot_len = 0; /* Filled later */
	iph->id = 0; /* (iph_g->id + 5000); */
	iph->frag_off = 0;
	iph->ttl = 128;
	iph->protocol = 1;
	iph->check = 0; // Filled later;
	iph->saddr = htonl(ip_address); 
	iph->daddr = iph_g->saddr; 
	/* iph->daddr = inet_addr("255.255.255.255"); */

	struct icmp_hdr *ich = (struct icmp_hdr *)(arp_icmp_reply + l2_hdr_size + l3_hdr_size);
	ich->icmp_type = ICMP_ECHOREPLY;
	ich->icmp_code = 0;
	ich->icmp_sum = 0;
	ich->id = icmp_hg->id;
	ich->seq = icmp_hg->seq;
	icmp_len = (ntohs(iph_g->tot_len) - (iph_g->ihl << 2) - ICMP_H);
	memcpy((((u_char *)&ich->seq) + 1), (((u_char *)&icmp_hg->seq) +1), (icmp_len + 1)); 
	iph->tot_len = htons((l3_hdr_size + ICMP_H + icmp_len));
	iph->check = ipchksum((u_int16_t *)(arp_icmp_reply + l2_hdr_size), iph->ihl << 1);
	ich->icmp_sum = icmpchksum((u_int16_t *)(arp_icmp_reply + l2_hdr_size + l3_hdr_size), ((icmp_len + ICMP_H) / 2)); 
    }
    return 0;
}
 
/*
 * Checks whether received packet is DHCP offer/ACK/NACK/ARP/ICMP
 * and retunrs the received packet type
 */
int check_packet(int pkt_type) 
{
    if(pkt_type == DHCP_MSGOFFER && vlan != 0) {
	map_all_layer_ptr(DHCP_MSGOFFER);
	if(ntohs(vlan_hg->vlan_priority_c_vid) == vlan && ntohs(vlan_hg->vlan_tpi) == ETHERTYPE_VLAN && iph_g->protocol == 17 && uh_g->source == htons(67) && uh_g->dest == htons(68)) {
	    if(*(dhopt_pointer_g + 2) == DHCP_MSGOFFER && htonl(dhcph_g->dhcp_xid) == dhcp_xid) {
		return DHCP_OFFR_RCVD;
	    } else {
		return UNKNOWN_PACKET;
	    }
	} else {
	    return UNKNOWN_PACKET;
	}
    } else if (pkt_type == DHCP_MSGACK && vlan != 0){
	map_all_layer_ptr(DHCP_MSGACK);
	if(ntohs(vlan_hg->vlan_priority_c_vid) == vlan && ntohs(vlan_hg->vlan_tpi) == ETHERTYPE_VLAN && iph_g->protocol == 17 && uh_g->source == htons(67) && uh_g->dest == htons(68)) {
	    if(*(dhopt_pointer_g + 2) == DHCP_MSGACK && htonl(dhcph_g->dhcp_xid) == dhcp_xid) {
		return DHCP_ACK_RCVD;
	    } else if(*(dhopt_pointer_g + 2) == DHCP_MSGNACK && htonl(dhcph_g->dhcp_xid) == dhcp_xid){
		return DHCP_NAK_RCVD;
	    } else {
		return UNKNOWN_PACKET;
	    }
	    
	} else {
	    return UNKNOWN_PACKET;
	}
    } else if (pkt_type == DHCP_MSGOFFER) {
	map_all_layer_ptr(DHCP_MSGOFFER);
	if(eth_hg->ether_type == htons(ETHERTYPE_IP) && iph_g->protocol == 17 && uh_g->source == htons(67) && uh_g->dest == htons(68)) {
	    if(*(dhopt_pointer_g + 2) == DHCP_MSGOFFER && htonl(dhcph_g->dhcp_xid) == dhcp_xid) {
		return DHCP_OFFR_RCVD;
	    } else {
		return UNKNOWN_PACKET;
	    }
	} else {
	    return UNKNOWN_PACKET;
	}

    } else if (pkt_type == DHCP_MSGACK) {
	map_all_layer_ptr(DHCP_MSGACK);
	if(eth_hg->ether_type == htons(ETHERTYPE_IP) && iph_g->protocol == 17 && uh_g->source == htons(67) && uh_g->dest == htons(68)) {
	    if(*(dhopt_pointer_g + 2) == DHCP_MSGACK && htonl(dhcph_g->dhcp_xid) == dhcp_xid) {
		return DHCP_ACK_RCVD;
	    } else if(*(dhopt_pointer_g + 2) == DHCP_MSGNACK && htonl(dhcph_g->dhcp_xid) == dhcp_xid) {
		return DHCP_NAK_RCVD;
	    } else {
		return UNKNOWN_PACKET;
	    }
	} else {
	    return UNKNOWN_PACKET;
	}
    } else if(pkt_type == ARP_ICMP_RCV) {
	map_all_layer_ptr(ARP_MAP); 
	if(!vlan) {
	    
	    if((ntohs(arp_hg->ar_op)) == ARPOP_REQUEST && (htonl(ip_address)) == (*((u_int32_t *)(arp_hg->target_ip)))) {
		return ARP_RCVD;
	    }
	} else if(vlan && ntohs(vlan) == vlan_hg->vlan_priority_c_vid) {
	    if((ntohs(arp_hg->ar_op)) == ARPOP_REQUEST && (htonl(ip_address)) == (*((u_int32_t *)(arp_hg->target_ip)))) {
		fprintf(stdout, "Arp request received\n"); 
		return ARP_RCVD;
	    }
	}
	map_all_layer_ptr(ICMP_MAP);
	if(!vlan) {
	    if((ntohs(eth_hg->ether_type)) == ETHERTYPE_IP && iph_g->protocol == 1 && ip_address == ntohl(iph_g->daddr) && icmp_hg->icmp_type == ICMP_ECHO) {
		return ICMP_RCVD;
	    }
	} else if(vlan && ntohs(vlan) == vlan_hg->vlan_priority_c_vid) {
	    if((ntohs(vlan_hg->vlan_len)) == ETHERTYPE_IP && iph_g->protocol == 1 && ip_address == ntohl(iph_g->daddr) && icmp_hg->icmp_type == ICMP_ECHO) {
		return ICMP_RCVD;
	    }
	}
	return UNKNOWN_PACKET;
    }
}

/*
 * Sets the server ip and offerered ip on serv_id, option50_ip
 * from the DHCP offer packet
 */
int set_serv_id_opt50()
{
    map_all_layer_ptr(DHCP_MSGOFFER);
    
    option50_ip = dhcph_g->dhcp_yip;

    while(*(dhopt_pointer_g) != DHCP_END) {
	if(*(dhopt_pointer_g) == DHCP_SERVIDENT) {
	    memcpy(&server_id, (u_int32_t *)(dhopt_pointer_g + 2), 4);
	}
	dhopt_pointer_g = dhopt_pointer_g + *(dhopt_pointer_g + 1) + 2;
    }
    return 0;
}

/*
 * Prints the DHCP offer/ack info
 */
int print_dhinfo(int pkt_type) 
{
    u_int16_t tmp;
    if(pkt_type == DHCP_MSGOFFER) {
	map_all_layer_ptr(DHCP_MSGOFFER);
	
	fprintf(stdout, "\nDHCP offer details\n");
	fprintf(stdout, "----------------------------------------------------------\n");
	fprintf(stdout, "DHCP offered IP from server - %s\n", inet_ntoa(dhcph_g->dhcp_yip));
	fprintf(stdout, "Next server IP(Probably TFTP server) - %s\n", inet_ntoa(dhcph_g->dhcp_sip));
	if(dhcph_g->dhcp_gip) {
	fprintf(stdout, "DHCP Relay agent IP - %s\n", inet_ntoa(dhcph_g->dhcp_gip));
	}
    } else if( pkt_type == DHCP_MSGACK) {
	map_all_layer_ptr(DHCP_MSGACK);
	
	fprintf(stdout, "\nDHCP ack details\n");
	fprintf(stdout, "----------------------------------------------------------\n");
	fprintf(stdout, "DHCP offered IP from server - %s\n", inet_ntoa(dhcph_g->dhcp_yip));
	fprintf(stdout, "Next server IP(Probably TFTP server) - %s\n", inet_ntoa(dhcph_g->dhcp_sip));
	if(dhcph_g->dhcp_gip) {
	fprintf(stdout, "DHCP Relay agent IP - %s\n", inet_ntoa(dhcph_g->dhcp_gip));
	}
    }

    while(*(dhopt_pointer_g) != DHCP_END) {

	switch(*(dhopt_pointer_g)) {
	    case DHCP_SERVIDENT:
	    fprintf(stdout, "DHCP server  - %s\n", inet_ntoa(*(u_int32_t *)(dhopt_pointer_g + 2)));
	    break;

	    case DHCP_LEASETIME: 
	    fprintf(stdout, "Lease time - %d Days %d Hours %d Minutes\n", \
	    (ntohl(*(u_int32_t *)(dhopt_pointer_g + 2))) / (3600 * 24), \
	    ((ntohl(*(u_int32_t *)(dhopt_pointer_g + 2))) % (3600 * 24)) / 3600, \
	    (((ntohl(*(u_int32_t *)(dhopt_pointer_g + 2))) % (3600 * 24)) % 3600) / 60); 
	    break;

	    case DHCP_SUBNETMASK:
	    fprintf(stdout, "Subnet mask - %s\n", inet_ntoa(*(u_int32_t *)(dhopt_pointer_g + 2)));
	    break;

	    case DHCP_ROUTER:
	    for(tmp = 0; tmp < (*(dhopt_pointer_g + 1) / 4); tmp++) {
	    fprintf(stdout, "Router/gateway - %s\n", inet_ntoa(*(u_int32_t *)(dhopt_pointer_g + 2 + (tmp * 4))));
	    }
	    break;

	    case DHCP_DNS:
	    for(tmp = 0; tmp < ((*(dhopt_pointer_g + 1)) / 4); tmp++) {
	    fprintf(stdout, "DNS server - %s\n", inet_ntoa(*(u_int32_t *)(dhopt_pointer_g + 2 + (tmp * 4))));
	    }
	    break;
	}
	
	dhopt_pointer_g = dhopt_pointer_g + *(dhopt_pointer_g + 1) + 2;
    }

    fprintf(stdout, "----------------------------------------------------------\n\n");
    return 0;
}

/*
 * Function maps all pointers on OFFER/ACK/ARP/ICMP packet
 */
int map_all_layer_ptr(int pkt_type)
{
    if(pkt_type == DHCP_MSGOFFER && vlan != 0) {
	vlan_hg = (struct vlan_hdr *)dhcp_packet_offer; 
	iph_g = (struct iphdr *)(dhcp_packet_offer + l2_hdr_size);
	uh_g = (struct udphdr *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size);
	dhcph_g = (struct dhcpv4_hdr *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size + l4_hdr_size);
	dhopt_pointer_g = (u_int8_t *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size + l4_hdr_size + sizeof(struct dhcpv4_hdr));
    } else if(pkt_type == DHCP_MSGOFFER && vlan == 0) {
	eth_hg = (struct ethernet_hdr *)dhcp_packet_offer;
	iph_g = (struct iphdr *)(dhcp_packet_offer + l2_hdr_size);
	uh_g = (struct udphdr *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size);
	dhcph_g = (struct dhcpv4_hdr *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size + l4_hdr_size);
	dhopt_pointer_g = (u_int8_t *)(dhcp_packet_offer + l2_hdr_size + l3_hdr_size + l4_hdr_size + sizeof(struct dhcpv4_hdr));
    } else if(pkt_type == DHCP_MSGACK && vlan != 0) {
	vlan_hg = (struct vlan_hdr *)dhcp_packet_ack; 
	iph_g = (struct iphdr *)(dhcp_packet_ack + l2_hdr_size);
	uh_g = (struct udphdr *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size);
	dhcph_g = (struct dhcpv4_hdr *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size + l4_hdr_size);
	dhopt_pointer_g = (u_int8_t *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size + l4_hdr_size + sizeof(struct dhcpv4_hdr));
    } else if(pkt_type == DHCP_MSGACK && vlan == 0) {
	eth_hg = (struct ethernet_hdr *)dhcp_packet_ack;
	iph_g = (struct iphdr *)(dhcp_packet_ack + l2_hdr_size);
	uh_g = (struct udphdr *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size);
	dhcph_g = (struct dhcpv4_hdr *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size + l4_hdr_size);
	dhopt_pointer_g = (u_int8_t *)(dhcp_packet_ack + l2_hdr_size + l3_hdr_size + l4_hdr_size + sizeof(struct dhcpv4_hdr));
    } else if(pkt_type == ARP_MAP && vlan != 0) {
	vlan_hg = (struct vlan_hdr *)arp_icmp_packet;
	arp_hg = (struct arp_hdr *)(arp_icmp_packet + l2_hdr_size);
    } else if(pkt_type == ARP_MAP && vlan == 0) {
	eth_hg = (struct ethernet_hdr *)arp_icmp_packet;
	arp_hg = (struct arp_hdr *)(arp_icmp_packet + l2_hdr_size);
    } else if(pkt_type == ICMP_MAP && vlan != 0) {
	vlan_hg = (struct vlan_hdr *)arp_icmp_packet;
	iph_g = (struct iphdr *)(arp_icmp_packet + l2_hdr_size);
	icmp_hg = (struct icmp_hdr *)(arp_icmp_packet + l2_hdr_size + l3_hdr_size);
    } else if(pkt_type == ICMP_MAP && vlan == 0) {
	eth_hg = (struct ethernet_hdr *)arp_icmp_packet;
	iph_g = (struct iphdr *)(arp_icmp_packet + l2_hdr_size);
	icmp_hg = (struct icmp_hdr *)(arp_icmp_packet + l2_hdr_size + l3_hdr_size);
    }
    return 0;
}

/*
 * Logs DHCP info to the log file
 * This file is used later for DHCP release
 */
int log_dhinfo()
{
    map_all_layer_ptr(DHCP_MSGACK);
    FILE *dh_file;
    
    dh_file = fopen(dhmac_fname, "w");
    if(dh_file == NULL) {
	perror("Error on opening file");
	exit(1);
    }
    if(!vlan) {
	fprintf(dh_file, "Client_mac: %s\n", dhmac_fname);
	fprintf(dh_file, "Acquired_ip: %s\n", inet_ntoa(dhcph_g->dhcp_yip));
	fprintf(dh_file, "Server_id: %s\n", inet_ntoa(server_id));
	fprintf(dh_file, "Host_mac: %02X:%02X:%02X:%02X:%02X:%02X\n", eth_hg->ether_shost[0],\
		eth_hg->ether_shost[1], eth_hg->ether_shost[2], eth_hg->ether_shost[3],\
		eth_hg->ether_shost[4], eth_hg->ether_shost[5]);
	ip_address = ntohl(dhcph_g->dhcp_yip);
	if(ip_listen_flag) {
	    fprintf(dh_file, "IP_listen: True. Pid: %d\n", getpid());
	} else {
	    fprintf(dh_file, "IP_listen: False. Pid: %d\n", 0);
	}
    } else {
	fprintf(dh_file, "Client_mac: %s\n", dhmac_fname);
	fprintf(dh_file, "Acquired_ip: %s\n", inet_ntoa(dhcph_g->dhcp_yip));
	fprintf(dh_file, "Server_id: %s\n", inet_ntoa(server_id));
	fprintf(dh_file, "Host_mac: %02X:%02X:%02X:%02X:%02X:%02X\n", vlan_hg->vlan_shost[0],\
		vlan_hg->vlan_shost[1], vlan_hg->vlan_shost[2], vlan_hg->vlan_shost[3],\
		vlan_hg->vlan_shost[4], vlan_hg->vlan_shost[5]);
	ip_address = ntohl(dhcph_g->dhcp_yip);
	if(ip_listen_flag) {
	    fprintf(dh_file, "IP_listen: True. Pid: %d\n", getpid());
	} else {
	    fprintf(dh_file, "IP_listen: False. Pid: %d\n", 0);
	}
    }
    fclose(dh_file);
    return 0;
}

/*
 * Takes the DHCP info from log file and removes it(unlinks it)
 * Used for DHCP release
 */
int get_dhinfo()
{
    FILE *dh_file;
    char mac_tmp[20], acq_ip_tmp[20], serv_id_tmp[20], dmac_tmp[20], ip_listen_tmp[10];
    pid_t dh_pid;
    dh_file = fopen(dhmac_fname, "r");
    if(dh_file == NULL) {
	return ERR_FILE_OPEN;
    }
    fscanf(dh_file, "Client_mac: %s\nAcquired_ip: %s\nServer_id: %s\n\
    Host_mac: %2X:%2X:%2X:%2X:%2X:%2X\nIP_listen: %s Pid: %d\n", mac_tmp, acq_ip_tmp, serv_id_tmp, \
    &dmac[0], &dmac[1], &dmac[2], &dmac[3], &dmac[4], &dmac[5], ip_listen_tmp, &dh_pid);
    option50_ip = inet_addr(acq_ip_tmp);
    server_id = inet_addr(serv_id_tmp);
    if((strncmp(ip_listen_tmp, "True", 4)) == 0) {
	kill(dh_pid, SIGKILL);	
    }
    fclose(dh_file);
    unlink(dhmac_fname);
    return 0;
}


