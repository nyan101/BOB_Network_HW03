#include <netinet/in.h> // for ntohs() function
#include <pcap.h>       // for packet capturing
#include <stdlib.h>
//for structure
#include <netinet/ether.h>
#include <arpa/inet.h>
//custom headerfile
#include "getLocalAddress.h"
#include "ARPtools.h"

int isPacketToRelay(u_char *packet, struct relaySession rSession)
{

}

void relayPackets(pcap_t *pcd, relaySession *relayList, int relayNum)
{
    struct ether_header *etherHdr;

    struct pcap_pkthdr *recvHeader;
    const u_char *recvPacket;
    u_char sendPacket[sizeof(struct ether_header) + sizeof(struct ether_arp)];

    while(1)
    {
        // spin lock, escape after getting Response
        while(pcap_next_ex(pcd, &recvHeader, &recvPacket) != 1) ;

        
        // check if it's a packet to relay
        for(i=0;i<relayNum;i++)
        {
            if(isPacketToRelay(recvPacket, relayList[i]))
            {
                // relay according to relayList[i]
            }
        }

    }

    return;
}

int convertIP2MAC(pcap_t *pcd, const struct in_addr IP, struct ether_addr *MAC)
{
    int status;
    struct ether_addr BcastMAC;
    struct ether_header *etherHdr;
    struct ether_arp *arpHdr;

    struct pcap_pkthdr *recvHeader;
    const u_char *recvPacket;
    u_char sendPacket[sizeof(struct ether_header) + sizeof(struct ether_arp)];

    // check if it's in the same network
    if((getMyAddr().IP.s_addr & getMyAddr().subMask.s_addr)
        != (IP.s_addr & getMyAddr().subMask.s_addr))
        return -1;

    // make ARP REQUEST packet
    ether_aton_r("ff:ff:ff:ff:ff:ff", &BcastMAC);
    makeARPpacket(sendPacket, getMyAddr().IP, getMyAddr().MAC, IP, BcastMAC, ARPOP_REQUEST);
    
    // send and get ARP response
    while(1)
    {
        // send Request
        if(pcap_inject(pcd, sendPacket, sizeof(sendPacket))==-1)
        {
            pcap_perror(pcd,0);
            pcap_close(pcd);
            exit(1);
        }

        // get Response
        status = pcap_next_ex(pcd, &recvHeader, &recvPacket);
        if(status!=1)
            continue;

        // check if it's ARP packet
        etherHdr = (struct ether_header*)recvPacket;
        if(etherHdr->ether_type!=htons(ETHERTYPE_ARP))
            continue;
        
        // check if it's 1)ARP Reply 2)from the desired source
        arpHdr = (struct ether_arp*)(recvPacket + sizeof(struct ether_header));
        if(arpHdr->arp_op != htons(ARPOP_REPLY))
            continue;
        if(memcmp(&arpHdr->arp_spa, &IP.s_addr, sizeof(in_addr_t))!=0)
            continue;

        // if so, copy MAC addr
        memcpy(&MAC->ether_addr_octet, &arpHdr->arp_sha, ETHER_ADDR_LEN);

        break;
    }


    return 0;
}

void makeARPpacket(u_char *packet, const struct in_addr sendIP, const struct ether_addr sendMAC,
                                   const struct in_addr recvIP, const struct ether_addr recvMAC, uint16_t ARPop)
{
    struct ether_header etherHdr;
    struct ether_arp arpHdr;

    // Ethernet part
    etherHdr.ether_type = htons(ETHERTYPE_ARP);
    memcpy(etherHdr.ether_dhost, &recvMAC.ether_addr_octet, ETHER_ADDR_LEN);
    memcpy(etherHdr.ether_shost, &sendMAC.ether_addr_octet, ETHER_ADDR_LEN);

    // ARP part
    arpHdr.arp_hrd = htons(ARPHRD_ETHER);
    arpHdr.arp_pro = htons(ETHERTYPE_IP);
    arpHdr.arp_hln = ETHER_ADDR_LEN;
    arpHdr.arp_pln = sizeof(in_addr_t);
    arpHdr.arp_op  = htons(ARPop);
    memcpy(&arpHdr.arp_sha, &sendMAC.ether_addr_octet, ETHER_ADDR_LEN);
    memcpy(&arpHdr.arp_spa, &sendIP.s_addr, sizeof(in_addr_t));
    memcpy(&arpHdr.arp_tha, &recvMAC.ether_addr_octet, ETHER_ADDR_LEN);
    memcpy(&arpHdr.arp_tpa, &recvIP.s_addr, sizeof(in_addr_t));

    // build packet
    memcpy(packet, &etherHdr, sizeof(struct ether_header));
    memcpy(packet+sizeof(struct ether_header), &arpHdr, sizeof(struct ether_arp));

    return;
}