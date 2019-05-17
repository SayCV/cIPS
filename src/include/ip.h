#ident "@(#) $Id: ip.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
#ifndef __IP_H__
#define __IP_H__

#include "arch.h"
#include "basic_c_types.h"

struct NETIF_S;

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Function name: ip_parse
 * \return ERR_CHECKSUM, ERR_OK, ERR_CHECKSUM or ERR_DEVICE_DRIVER
 * \param eth_frame : [in] Ethernet frame.
 * \param eth_frame_length : [in] Ethernet frame length.
 * \param net_adapter : [in] Adpater providing the frame.
 * \brief Parse the IP frame.
 * *******************************************************************/
  err_t ip_parse(u8_t* eth_frame, u32_t eth_frame_length, struct NETIF_S *net_adapter);

/*!
 * Function name: eth_build_ip_request
 * \return length of the IP frame.
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param ip_output_frame : [out] Frame generated starting at the IP header to the end of the app data.
 * \param transport_length : [in] Length of Udp (or TCP) header and app data.
 * \param protocol : [in] IP_UDP or IP_TCP. 
 * \param reuse : [in] Flag. If set to TRUE, constant fields are not set again. 
 * \brief Build the IP frame. 
 * *******************************************************************/
 void eth_build_ip_request( const u32_t dest_ip_addr, const u32_t source_ip_addr, u8_t* const ip_output_frame, const u32_t transport_length, const u8_t protocol, const bool_t reuse);

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
 u16_t eth_build_pseudo_header( const u32_t dest_ip_addr, const u32_t source_ip_addr,const u32_t data_size, const u32_t proto_length, const u8_t protocol); 


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
u16_t ip_checksum(const u16_t *ipHeader, u32_t byte_nb);

/*!
 * Function name: icmp_ping
 * \return ERR_OK, ERR_VAL or ERR_MAC_ADDR_UNKNOWN.
 * \param net_adapter : [in/out] adapter of interest.
 * \param remote_ip : [in] IP address of the peer device that cIPS should reach.
 * \param app_data : [in] data to send (optional).
 * \param app_data_length : [in] data length (optional).
 * \brief Build the ICMP echo request frame. 
 * *******************************************************************/
err_t ip_icmp_ping(struct NETIF_S *net_adapter, u32_t remote_ip, const u8_t* const app_data, const u32_t app_data_length);

/*!
 * Function name: ip_set_constant_fields
 * \return nothing.
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param ip_output_frame : [out] Frame generated starting at the IP header to the end of the app data.
 * \param protocol : [in] IP_UDP or IP_TCP. 
 * \brief Initialize fields that are staying constant for the life of the connection
 * *******************************************************************/
void ip_set_constant_fields( const u32_t dest_ip_addr, const u32_t source_ip_addr, u8_t* const ip_output_frame, const u8_t protocol);

#ifdef __cplusplus
}
#endif

#endif /* __IP_H__ */


