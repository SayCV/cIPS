#ident "@(#) $Id: ip.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
/* Copyright (c) 2010,  Jean-Marc David
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without 
* modification, are permitted provided that the following conditions 
* are met:
*  * Redistributions of source code must retain the above copyright 
* notice, this list of conditions and the following disclaimer.
*  * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the 
* documentation and/or other materials provided with the distribution.
*  * Neither the name of the author nor the names of its contributors 
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, 
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
*  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

* Author: Jean-Marc David jmdavid1789<at>googlemail.com
* http://sourceforge.net/projects/cipsuite/ <br>
*/

/*!
* \namespace ip
* \file ip.c
* \brief  Module Description: IP stack.
***************************************************/

#include "basic_c_types.h"
#include "arp.h"
#include "netif.h"
#include "tcp.h"
#include "udp.h"
#include "err.h"
#include "nw_protocols.h"
#include "debug.h"
#include "ip.h"

static const u8_t IP_TTL = 255; //!< IP time_to_live field.
static err_t ip_parse_icmp(u8_t* eth_frame, u32_t eth_frame_length, NETIF_T *net_adapter);
static u32_t eth_build_icmp_echo_request(unsigned char* const output_frame, const u8_t* const app_data, const u32_t app_data_length);
static u32_t eth_build_icmp_response(u8_t* const io_frame, const u32_t app_length);
static err_t icmp_build_echo_reply_frame (u8_t* eth_frame, NETIF_T *net_adapter);
static void eth_swap(unsigned char* const io_frame, NETIF_T *net_adapter);
static void eth_memcpy(u8_t* output, const u8_t* input);
#if IP_DEBUG
static void ip_debug_print_src_dst(NETIF_T *net_adapter, u32_t src, u32_t dest);
#else
#  define ip_debug_print_src_dst(net_adapter, src,dest) 1
#endif /* IP_DEBUG */


/*!
 * Function name: ip_parse
 * \return ERR_CHECKSUM, ERR_OK, ERR_CHECKSUM or ERR_DEVICE_DRIVER
 * \param eth_frame : [in] Ethernet frame.
 * \param eth_frame_length : [in] Ethernet frame length.
 * \param net_adapter : [in] Adpater providing the frame.
 * \brief Parse the IP frame.
 * *******************************************************************/
err_t ip_parse(u8_t* eth_frame, u32_t eth_frame_length, NETIF_T *net_adapter)
{
  u16_t checksum;
  IP_HEADER_T* iphdr; 
  err_t err = ERR_OK;

  iphdr = (IP_HEADER_T*) (eth_frame + sizeof(ETHER_HEADER_T)); 

  (void)ip_debug_print_src_dst(net_adapter,ntohl(iphdr->source_addr),ntohl(iphdr->dest_addr));

  if(IP_CHECK_IP_VERSION(iphdr) == IP_VERSION){
    u32_t ip_header_length = IP_GET_HEADER_LENGTH(iphdr);
    //Verify IP checksum.
    checksum = ip_checksum((const u16_t*)iphdr, ip_header_length);

    if( checksum == CHECKSUM_OK ) {
      switch(iphdr->protocol) {
        case IP_UDP:
          err = udp_parse((u8_t*)iphdr, eth_frame_length - sizeof(ETHER_HEADER_T), net_adapter);
          break;
        case IP_TCP:
          err = tcp_demultiplex((u8_t*)iphdr, eth_frame_length - sizeof(ETHER_HEADER_T), net_adapter);
          break;
        case IP_ICMP: //if ICMP (AKA ping) is for us
          T_DEBUGF(IP_DEBUG, ("%s: ICMP (ping)\r\n",net_adapter->name));
          err = ip_parse_icmp(eth_frame, eth_frame_length, net_adapter);
          break;
        default:
          T_DEBUGF(IP_DEBUG, ("NOT SUPPORTED on interface %s\r\n",net_adapter->name));
        break;
      };
      T_DEBUGF(IP_DEBUG, ("\r\n"));
    } else {
      T_DEBUGF(IP_DEBUG,("%s :IP: checksum error, frame dropped\r\n",net_adapter->name));
      err =adapter_store_error( ERR_CHECKSUM, net_adapter, __func__, __LINE__);
    }
  } else {
    T_DEBUGF(IP_DEBUG, ("%s: IP frame is not IPv4\r\n",net_adapter->name));
  }
  return err;
}

/*!
 * Function name: ip_parse_icmp
 * \return ERR_CHECKSUM or ERR_OK
 * \param eth_frame : [in] Ethernet frame.
 * \param eth_frame_length : [in] Ethernet frame length.
 * \param net_adapter : [in] Adpater providing the frame.
 * \brief Parse the ICMP frame.
 * *******************************************************************/
static err_t ip_parse_icmp(u8_t* eth_frame, u32_t eth_frame_length, NETIF_T *net_adapter)
{
  IP_HEADER_T* iphdr; 
  ICMP_HEADER_T* icmphdr;
  u32_t ip_header_length;
  err_t err = ERR_OK;

  iphdr = (IP_HEADER_T*) (eth_frame + sizeof(ETHER_HEADER_T)); 
  ip_header_length = IP_GET_HEADER_LENGTH(iphdr);
  icmphdr = (ICMP_HEADER_T*) (eth_frame + sizeof(ETHER_HEADER_T) + ip_header_length); 

  switch(icmphdr->type)
  {
    case ICMP_ECHOREQUEST: //The peer device sends a ping to cIPS. cIPS replies.
      T_DEBUGF(IP_DEBUG, ("%s: ICMP (ping)\r\n",net_adapter->name));
      err = icmp_build_echo_reply_frame(eth_frame, net_adapter);
      if(net_adapter->ping_received != NULL)
      {
         net_adapter->ping_received(net_adapter, net_adapter->callback_arg, eth_frame + sizeof(ETHER_HEADER_T) + ip_header_length + sizeof(ICMP_ECHO_HEADER_T), ntohs(iphdr->length) - (ip_header_length + sizeof(ICMP_ECHO_HEADER_T)));
      }
    break;
    case ICMP_ECHOREPLY: //cIPS sent a ping to a peer device. The peer device replied and here is the reply processing.
      T_DEBUGF(IP_DEBUG, ("%s: ICMP (ping response)\r\n",net_adapter->name));
      if(net_adapter->ping_reply_received != NULL)
      {
         net_adapter->ping_reply_received(net_adapter, net_adapter->callback_arg, eth_frame + sizeof(ETHER_HEADER_T) + ip_header_length + sizeof(ICMP_ECHO_HEADER_T), ntohs(iphdr->length) - (ip_header_length + sizeof(ICMP_ECHO_HEADER_T)));
      }
    break;
    default:
    T_DEBUGF(IP_DEBUG, ("ICMP type NOT SUPPORTED on interface %s\r\n",net_adapter->name));
    break;
  };
  T_DEBUGF(IP_DEBUG, ("\r\n"));

  return err;
}

/*!
 * Function name: icmp_build_echo_reply_frame
 * \return XST_SUCCESS (=ERR_OK) or ERR_DEVICE_DRIVER.
 * \param eth_frame : [in/out] Ethernet Frame generated.
 * \param net_adapter : [in] adapter of interest.
 * \brief descriptions: Following reception of a PING, it builds the reply.
 * \note : Reuse the same frame.
 * *******************************************************************/
static err_t icmp_build_echo_reply_frame (u8_t* eth_frame, NETIF_T *net_adapter)
{
  u32_t length;
  u32_t app_length;
  u32_t ip_header_length;
  IP_HEADER_T *iphdr;
  err_t err;

  //1.Fill in ethernet part
  (void) eth_swap(eth_frame, net_adapter);

  //2.Fill in ICMP part
  iphdr = (IP_HEADER_T *)(eth_frame + sizeof(ETHER_HEADER_T));
  ip_header_length = IP_GET_HEADER_LENGTH(iphdr);
  app_length = ntohs(iphdr->length) - (ip_header_length + sizeof(ICMP_HEADER_T));
  length = eth_build_icmp_response(eth_frame + sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T),app_length);

  //If the incoming frame had ip options then shift the application data part because the reply will not have ip options.
  if( ip_header_length != sizeof(IP_HEADER_T))
  {
    u32_t i;
    u8_t* src;
    u8_t* dst;
    src = eth_frame + sizeof(ETHER_HEADER_T) + ip_header_length + sizeof(ICMP_HEADER_T);
    dst = eth_frame + sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ICMP_HEADER_T);
    for (i=0; i< app_length ; i++)
    {
      dst[i] = src[i];
    }
  }
  //3.Fill in IP part
  (void)eth_build_ip_request( htonl(iphdr->source_addr), htonl(iphdr->dest_addr), 
    eth_frame + sizeof(ETHER_HEADER_T), length, IP_ICMP, 0 /*reuse*/);

  //4.Send to the network
  length += sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T);
  err = netif_send(net_adapter,eth_frame, length);
  return err;
}

/*!
 * Function name: eth_build_icmp_response
 * \return length of the ICMP frame.
 * \param io_frame : [out] Ethernet Frame generated.
 * \param app_length : [in] ICMP data + (identifier sequence nb).
 * \brief descriptions: Build the ICMP echo reply frame.
 * *******************************************************************/
static u32_t eth_build_icmp_response(u8_t* const io_frame, const u32_t app_length)
{
  ICMP_HEADER_T* icmp;
  u32_t length;

  //Move to the ICMP portion of the packet and populate it appropriately
  icmp = (ICMP_HEADER_T*)(io_frame);
  icmp->type = ICMP_ECHOREPLY; // type of message
  icmp->cksum = 0; //Reset before scanning the reused frame
  length= sizeof(ICMP_HEADER_T) + app_length;
  icmp->cksum = ~ip_checksum((const u16_t*)icmp, length); // ones complement cksum of struct

  return length;
}

/*!
 * Function name: icmp_ping
 * \return ERR_OK, ERR_VAL or ERR_MAC_ADDR_UNKNOWN.
 * \param net_adapter : [in/out] adapter of interest.
 * \param remote_ip : [in] IP address of the peer device that cIPS should reach.
 * \param app_data : [in] data to send (optional).
 * \param app_data_length : [in] data length (optional).
 * \brief descriptions: Build the ICMP echo request frame. 
 * *******************************************************************/
err_t ip_icmp_ping(NETIF_T *net_adapter, u32_t remote_ip, const u8_t* const app_data, const u32_t app_data_length)
{
  const u32_t MIN_PING_DATA_LENGTH = 32;  //!< Minimun size for a PING accepted by the network.
  u32_t framelen;
  u8_t dest_mac_addr[MAC_ADDRESS_LENGTH];
  u32_t dest_ip_or_gateway;
  err_t err = ERR_OK;
  u8_t* eth_frame = net_adapter->control_buffer;
  u32_t min_app_data_length = (app_data_length > MIN_PING_DATA_LENGTH)?app_data_length:MIN_PING_DATA_LENGTH;
  
  if ((remote_ip == 0) || (net_adapter == NULL))
  {
    err =adapter_store_error( ERR_VAL, net_adapter, __func__, __LINE__);
    return err;
  }

  dest_ip_or_gateway = arp_get_route(remote_ip, net_adapter->gateway_addr,net_adapter->netmask);

  if (arp_query_cache(&(net_adapter->arp_cache), dest_ip_or_gateway, dest_mac_addr, net_adapter->netmask) != ERR_VAL)
  {
    //1.Fill in ethernet part
     framelen = eth_build_frame( dest_mac_addr, net_adapter->mac_address, remote_ip, net_adapter->ip_addr, eth_frame, ETH_ETHERNET);

    //2.Fill in ICMP part
    framelen = eth_build_icmp_echo_request(eth_frame + sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T), app_data, min_app_data_length);

    //3.Fill in IP part
    (void)eth_build_ip_request( remote_ip, net_adapter->ip_addr,
        eth_frame + sizeof(ETHER_HEADER_T), framelen, IP_ICMP, 0 /*reuse*/);

    //4.Send to the network
    framelen += sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T);
    err = netif_send(net_adapter,eth_frame, framelen);
  }
  else
  { //The destination MAC address is unknow, send an ARP request to resolve it.
    framelen = eth_build_frame( dest_mac_addr, net_adapter->mac_address, dest_ip_or_gateway, net_adapter->ip_addr, eth_frame, ETH_ARP_REQUEST);
    (void)netif_send(net_adapter,eth_frame, framelen);
    err =adapter_store_error( ERR_MAC_ADDR_UNKNOWN, net_adapter, __func__, __LINE__);
  }
  return err;
}

/*!
 * Function name: eth_build_icmp_echo_request
 * \return length of the ICMP frame.
 * \param output_frame : [out] Ethernet Frame generated.
 * \param app_data : [in] data to send.
 * \param app_data_length : [in] data length.
 * \brief descriptions: Build the ICMP echo request frame.
 * *******************************************************************/
static u32_t eth_build_icmp_echo_request(unsigned char* const output_frame, const u8_t* const app_data, const u32_t app_data_length)
{
  #define ICMP_ECHO_ID  0x2222
  ICMP_ECHO_HEADER_T* icmp;
  u8_t* data;
  u32_t i;
  u32_t length;

  //Move to the ICMP portion of the packet and populate it appropriately  
  icmp = (ICMP_ECHO_HEADER_T*)(output_frame); 
  icmp->type = ICMP_ECHOREQUEST;  // type of message 
  icmp->code = 0;  // type subcode 
  icmp->id = htons(ICMP_ECHO_ID);// identifier (arbitrary to the stack)
  icmp->seq = htons(app_data_length);  // sequence number. "app_data_length" is used as a random number. It has no relationship with a sequence number

  //Add "app_data_length" bytes of data... 
  data = output_frame + sizeof(ICMP_ECHO_HEADER_T);
  for(i=0;i<app_data_length;i++)
  {
    data[i] = app_data[i];
  }
  icmp->cksum = 0; //Reset before scanning the reused frame
  length= sizeof(ICMP_ECHO_HEADER_T) + app_data_length;
  icmp->cksum = ~ip_checksum((const u16_t*)icmp, length); // ones complement cksum of struct 

  return length;
}

/*!
 * Function name: eth_swap
 * \return nothing.
 * \param io_frame : [in/out] Ethernet Frame generated.
  * \param net_adapter : [in] adapter of interest.
 * \brief Prepare an Ethernet reply by swapping the destination and
 * source addresses.
 * *******************************************************************/
static void eth_swap(u8_t* const io_frame, NETIF_T *net_adapter)
{
  ETHER_HEADER_T* ether = (ETHER_HEADER_T *)io_frame;

  (void)eth_memcpy( ether->destination_addr, ether->source_addr);
  (void)eth_memcpy( ether->source_addr, net_adapter->mac_address);

  return;
}

/*!
 * Function name: eth_memcpy
 * \return nothing.
 * \param output : [in] Array to copy into.
 * \param input : [in]  Array to be copied.
 * \brief Copy each elt of one MAC address array to another one.
 * *******************************************************************/
static void eth_memcpy(u8_t* output, const u8_t* input)
{
  int i=MAC_ADDRESS_LENGTH;
  do {
    *output++ = *input++;
  } while(--i); 
  
  return;
}


/*!
 * Function name: eth_build_pseudo_header
 * \return Sum of the speudo header elts (in big endian).
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param data_size : [in] Size in bytes of the "application data".
 * \param proto_length : [in] sizeof(struct udp_hdr) or sizeof(struct tcp_hdr).
 * \param protocol : [in] IP_UDP or IP_TCP.
 * \brief Build a speudo header (UDP or TCP) and adds its elements
 * to prepare the checksum calculation. According to the RFC793, it gives
 * a "protection against misrouted segments".
 * \note all the parameters are in the the endianness of the platform.
 * cIPs does the conversion before calling this function.
 * *******************************************************************/
u16_t eth_build_pseudo_header( const u32_t dest_ip_addr, const u32_t source_ip_addr,
  const u32_t data_size, const u32_t proto_length, const u8_t protocol)
{
  TRANSPORT_PSEUDO_HEADER_T tp; //!< transport_protocol : UDP or TCP
  u32_t cksum;

  tp.ip_src = source_ip_addr; //source address
  tp.ip_dst = dest_ip_addr; //dest address
  tp.zero = 0; //fixed to zero
  tp.proto = protocol; 
  tp.ulen = data_size + proto_length;

  cksum = ((u16_t)(tp.ip_src >> 16)) + ((u16_t)tp.ip_src);
  cksum += ((u16_t)(tp.ip_dst >> 16)) + ((u16_t)tp.ip_dst);
  cksum += ((u16_t)tp.proto) + tp.ulen;

  //Fold 32-bit sum to 16 bits
  while( cksum >> 16) {
    cksum = (cksum & 0xFFFF) + (cksum >> 16);
  }

  //Return the 16-bit result in big endian.
  return htons((u16_t)cksum);
}

/*!
 * Function name: ip_checksum
 * \return checksum value (in big endian).
 * \param big_endian_frame : [in] Pointer to a big endian buffer.
 * The ethernet frame comes from the network as big endian.
 * cIPS stores it in a buffer as it is. This function works 
 * on that buffer so on big endian data.
 * \param byte_nb : [in] Size in "bytes" of "big_endian_frame".
 * \brief Compute the 16-bit one's complement checksum
 * \note the complement is not done and should done outside if needed.
 * *******************************************************************/
u16_t ip_checksum(const u16_t *big_endian_frame, u32_t byte_nb)
{
  u32_t cksum = 0;

  //IP headers always contain an even number of bytes.
  while (byte_nb > 1)
  {
    cksum += *big_endian_frame++;
    byte_nb -= sizeof(u16_t);
  }

  //Add left-over byte if any
  if( byte_nb > 0) {
#ifdef __BIG_ENDIAN__
    cksum += (*big_endian_frame) & 0xFF00; //Keep MSB of an unsigned short big endian.
#else
    cksum += (*big_endian_frame) & 0x00FF; //Keep MSB of an unsigned short big endian.
#endif
  }
  //Fold 32-bit sum to 16 bits
  while( cksum >> 16) {
    cksum = (cksum & 0xFFFF) + (cksum >> 16);
  }

  //Return the 16-bit result in big endian.
  return ((u16_t)cksum);
}

/*!
 * Function name: eth_build_ip_request
 * \return length of the IP frame.
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param ip_output_frame : [out] Frame generated starting at the IP header to the end of the app data.
 * \param transport_length : [in] Length of Udp (or TCP) header and app data.
 * \param protocol : [in] IP_UDP or IP_TCP. 
 * \param reuse : [in] Flag. If set to TRUE, constant fields are not set again. 
 * \brief descriptions: Build the IP frame. 
 * *******************************************************************/
void eth_build_ip_request(
  const u32_t dest_ip_addr, const u32_t source_ip_addr,
  u8_t* const ip_output_frame, const u32_t transport_length,
  const u8_t protocol, const bool_t reuse)
{
  IP_HEADER_T* ip = (IP_HEADER_T*)ip_output_frame;

  if( !reuse )
  {
    //Move to the IP portion of the packet and populate it appropriately
    ip->version_head_length = (IP_VERSION<<4)|(sizeof(IP_HEADER_T)>>2);
    ip->type_of_service = 0;
    ip->id = 0; //id;
    ip->fragment_offset_field = htons(IP_NO_FRAGMENT);
    ip->time_to_live = IP_TTL;
    ip->protocol = protocol;
    ip->source_addr = htonl(source_ip_addr);
    ip->dest_addr = htonl(dest_ip_addr);
  }
  ip->length = htons( sizeof(IP_HEADER_T) + transport_length); //IP frame length.
  ip->checksum = 0;//Reset before scanning the frame
  ip->checksum = ~ip_checksum((const u16_t*)ip, sizeof(IP_HEADER_T));

  return ;
}

/*!
 * Function name: ip_set_constant_fields
 * \return nothing.
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param ip_output_frame : [out] Frame generated starting at the IP header to the end of the app data.
 * \param protocol : [in] IP_UDP or IP_TCP. 
 * \brief Initialize fields that are staying constant for the life of the connection
 * *******************************************************************/
void ip_set_constant_fields(
  const u32_t dest_ip_addr, const u32_t source_ip_addr,
  u8_t* const ip_output_frame,
  const u8_t protocol)
{
  IP_HEADER_T* ip = (IP_HEADER_T*)ip_output_frame;

  //Move to the IP portion of the packet and populate it appropriately
  ip->version_head_length = (IP_VERSION<<4)|(sizeof(IP_HEADER_T)>>2);
  ip->type_of_service = 0;
  ip->id = 0; //id;
  ip->fragment_offset_field = htons(IP_NO_FRAGMENT);
  ip->time_to_live = IP_TTL;
  ip->protocol = protocol;
  ip->source_addr = htonl(source_ip_addr);
  ip->dest_addr = htonl(dest_ip_addr);

  return ;
}
#if IP_DEBUG
static void ip_debug_print_src_dst(NETIF_T *net_adapter, u32_t src, u32_t dest)
{
  T_DEBUGF(IP_DEBUG, ("%s     : ", net_adapter->name));
  T_DEBUGF(IP_DEBUG, ("IP from %ld.%ld.%ld.%ld ", (src >> 24) & 0xff, (src >> 16) & 0xff, (src >> 8) & 0xff, src & 0xff));
  T_DEBUGF(IP_DEBUG, ("to %ld.%ld.%ld.%ld\r\n", (dest >> 24) & 0xff, (dest >> 16) & 0xff, (dest >> 8) & 0xff, dest & 0xff));
}
#endif /* IP_DEBUG */

