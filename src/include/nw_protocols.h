#ident "@(#) $Id: nw_protocols.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
* \namespace  nw
* \file nw_protocols.h 
* \brief  Module Description: 
* Define the network characteristics (ethernet, IP, UDP...).<br>
***************************************************/
#ifndef _NW_PROTOCOLS_H_
#define _NW_PROTOCOLS_H_

/* libc Includes */

/* External Declarations */

/* Internal Declarations */

#define MAC_ADDRESS_LENGTH 6
#define IP_ADDRESS_LENGTH 4
#define ETHER_CRC_LENGTH 4 //!< Checksum length of the end of the ethernet frame.
#define CHECKSUM_OK 0xFFFF //!< If equal to that value, the checksum is correct 

//!Protocol types
#define ETHERTYPE_ETHERNET      0x0001          //!< Ethernet protocol 
#define ETHERTYPE_IP            0x0800          //!< IP protocol 
#define ETHERTYPE_ARP           0x0806          //!< Addr resolution protocol 

typedef struct __attribute__((packed)) ETHER_HEADER_S {
  u8_t destination_addr[MAC_ADDRESS_LENGTH]; //!<Destination address (see protocol definition).
  u8_t source_addr[MAC_ADDRESS_LENGTH]; //!<Source address
  u16_t frame_type; //!<Frame type (ETHERTYPE_IP, ETHERTYPE_ARP...) 
} ETHER_HEADER_T;

typedef struct __attribute__((packed)) ARP_HEADER_S{
  u16_t hardware; //!<Hardware type (ethernet)
  u16_t protocol; //!<Protocol type (IP)
  u8_t  hw_size; //!<Hardware size (MAC addr size)
  u8_t  protocol_size; //!<Protocol size
  u16_t operation;  //!<Operation: ARP_REQUEST, ARP_RESPONSE 
  u8_t  sender_hw_addr[MAC_ADDRESS_LENGTH]; //!<Sender MAC address
  u32_t sender_ip_addr; //!<Sender IP address
  u8_t  target_hw_addr[MAC_ADDRESS_LENGTH];//!<Target MAC address
  u32_t target_ip_addr;//!<Target IP address
} ARP_HEADER_T;

//! ARP operations:
#define ARP_REQUEST  1
#define ARP_RESPONSE 2

//!IP description
#define IP_VERSION  0x4
#define IP_NO_FRAGMENT 0x4000 // no fragment flag
#define IP_VERSION_MASK  0x40
#define IP_HEADER_LENGTH_MASK  0xF
#define IP_GET_HEADER_LENGTH(x) (((x)->version_head_length & IP_HEADER_LENGTH_MASK)<<2) //in bytes (<<2 stands for "*sizeof(u32_t)
#define IP_CHECK_IP_VERSION(x) (((x)->version_head_length & IP_VERSION_MASK) >> 4)

typedef struct __attribute__((packed)) IP_HEADER_S{
  u8_t version_head_length;      //!< version & header length 
  u8_t type_of_service;      //!< type of service 
  u16_t length;              //!< total length : IP_HEADER_T + upper layer 
  u16_t id;       //!< identification 
  u16_t fragment_offset_field; //!< fragment offset field 
  u8_t time_to_live;      //!< time to live 
  u8_t protocol;        //!< protocol 
  u16_t checksum;      //!< checksum 
  u32_t source_addr;  //!< source address
  u32_t dest_addr;  //!< dest address
} IP_HEADER_T;


//! Pseudo header for TCP or UDP cheksum.
typedef struct __attribute__((packed)) TRANSPORT_PSEUDO_HEADER_S{
  u32_t ip_src; //!<source address
  u32_t ip_dst; //!<dest address
  u8_t zero; //!<fixed to zero
  u8_t proto; //!<protocol
  u16_t ulen; //!<transport (udp or tcp) length
} TRANSPORT_PSEUDO_HEADER_T;

//! IP protocols
#define IP_ICMP 1
#define IP_TCP  6
#define IP_UDP  17

typedef struct __attribute__((packed)) ETHER_IP_HEADER_S{
  ETHER_HEADER_T eth;
  IP_HEADER_T ip;
} ETHER_IP_HEADER_T;


//! ICMP.
typedef struct __attribute__((packed)) ICMP_HEADER_S{
  u8_t  type;       //!< type of message
  u8_t  code;       //!< type subcode
  u16_t cksum;      //!< ones complement cksum of struct
} ICMP_HEADER_T;

//! ICMP echo reply.
typedef struct __attribute__((packed)) ICMP_ECHO_HEADER_S{
    u8_t   type;  //!< type of message
    u8_t   code;  //!< type subcode
    u16_t  cksum; //!< ones complement cksum of struct
    u16_t  id;    //!< identifier
    u16_t  seq;   //!< sequence number
} ICMP_ECHO_HEADER_T;

//! ICMP types
#define ICMP_ECHOREPLY          0
#define ICMP_ECHOREQUEST        8

//! UDP protocol header.
typedef struct __attribute__((packed)) UDP_HEADER_S{
  u16_t source_port; //!<source port
  u16_t dest_port; //!<destination port
  u16_t length; //!<length of the whole datagram ( UDP_HEADER_S + payload)
  u16_t chksum; //!<udp checksum
} UDP_HEADER_T;

/*! TCP */
typedef struct __attribute__((packed)) TCP_HEADER_S{
    u16_t  source_port; //!< Source port
    u16_t  dest_port;  //!< Source port
    u32_t  seqno;  //!< Sequence number
    u32_t  ackno;  //!< Acknowledgment number
    u16_t  data_offset_flags;  //!< Flags (lower 6 bits)
    u16_t  windowsize; //!< Window size
    u16_t  chksum;  //!< TCP checksum
    u16_t  urgent_ptr; //!< Urgent pointer
} TCP_HEADER_T;


#endif //_NW_PROTOCOLS_H_
