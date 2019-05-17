#ident "@(#) $Id: udp.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
#ifndef __UDP_H__
#define __UDP_H__

#include "arch.h"

struct NETIF_S;

typedef struct UDP_S {
  u32_t local_ip; //!<CIPS ip address
  u32_t remote_ip; //!<The peer ip address
  u16_t local_port; //!<CIPS TCP port
  u16_t remote_port; //!<The peer TCP port
  struct UDP_S *next; //!< for the linked list
  struct NETIF_S *netif; //!< network interface for this packet
  u32_t state;  //!< State: UDP_UNUSED, UDP_KNOWN_TARGET, UDP_ANY_TARGET
  u8_t frame[NETWORK_MTU]; //!< ethernet frame.*/
  bool_t point_to_point; //!< Flag. If TRUE, the link is point to point and no UDP checksum is processed. The ethernet and IP checksums are enough to guaranty the integrity of the frame.
  u8_t target_mac_addr[MAC_ADDRESS_LENGTH];
  u16_t chksum_len; //!< Spseudo-checksum length used when the frame is re-used.
  u8_t* app_data; //!< pointer to the application section within the ethernet frame "frame".
  bool_t frame_initialized; //!< In a situation where a UDP socket only sends frames of fix size, the length and the pseudo-checksum is constant so do not recaluclate it each time. TRUE means initailized.
  err_t (*recv)(void *arg, struct UDP_S *udp_c, void* data, u32_t data_length); //!< Callback when data have been received
  void *recv_arg; //!< argument associated to the "recv" callback.
} UDP_T;

#ifdef __cplusplus
extern "C" {
#endif

 /* Application interface. */

/*!
 * Function name: udp_new
 * \return: the udp_c created or NULL if no more controllers can be created.
 * \param ipaddr: [in] IP address linked to that udp_c.
 * \param port: [in] Port number linked to that udp_c.
 * \param point_to_point: [in] Flag. If TRUE, the link is point to point and no 
 * UDP checksum is processed. The ethernet and IP checksums are enough 
 * to guaranty the integrity of the frame.
 * \param err: [out]ERR_UDP_MEM if more than MAX_UDP udp connection already exist,
 * ERR_VAL if the IP@ is wrong, ERR_OK otherwise
 * \brief Create a new UDP Protocol Control Block.
 * *******************************************************************/
UDP_T* udp_new( u32_t ipaddr, u16_t port, bool_t point_to_point, err_t* err);

/*!
 * Function name: udp_connect
 * \return ERR_OK, ERR_VAL, ERR_MAC_ADDR_UNKNOWN
 * \param udp_c : [in/out] udp_c of interest.
 * \param ipaddr : [in] remote IP address to reach.
 * \param port : [in] remote port to reach.
 * \brief Connect a remote socket.
 * \note The application wants to reach a peer. But cIPS does not
 * know the peer's MAC address. The application calls
 * udp_connect() in order to resolve the MAC addres.
 * The MAC address can be in the ARP table or can be resolve
 * by an ARP request.
 * *******************************************************************/
  err_t udp_connect( UDP_T* udp_c, u32_t ipaddr, u16_t port);

/*!
 * Function name: udp_recv
 * \return nothing.
 * \param udp_c : [in/out] controller of interest.
 * \param recv : [in] Received callback.
 * \param recv_arg : [in/out] Data associated to that udp_c.
 * \brief It is a link between cIPS and the application, It specifies 
 * the application function that cIPS should call when a UDP connection 
 * receives data.
 * *******************************************************************/
  void udp_recv( UDP_T* udp_c, err_t (* recv)(void *arg, UDP_T* udp_c, void* data, u32_t data_length), void* recv_arg);

/*!
 * Function name: udp_send
 * \return ERR_OK, ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param udp_c : [in] Descriptor block containing the buffer to 
 * send (udp_c->frame).
 * \param data : [in] Application data. If set to NULL then 
 * udp_c->app_data constitues the data and no copies in performed.
 * \param data_length : [in] Application data length in bytes.
 * \param reuse : [in] TRUE if the size of data and the dest does not 
 * change. It reuses most of the previous frame.
 * \brief The application uses udp_send() in order to send an ethernet 
 * frame (udp_c->frame) to the network. ( udp_c->frame includes the 
 * app data udp_c->app_data).
 * \note udp_send_to is an encapsulation of udp_new, udp_connect, 
 * udp_send and udp_close
 * *******************************************************************/
  err_t udp_send( UDP_T* udp_c, void* data, u32_t data_length, bool_t reuse);

/*!
 * Function name: udp_delete
 * \return nothing.
 * \param udp_c: [in/out] connection of interest.
 * \brief udp_delete() is the opposite of udp_new(). udp_new() allocates
 * the resource and configures it (IP address, port#...). udp_delete()
 * frees the resource. It implies that the resource can be reused.
 * *******************************************************************/
  void udp_delete( UDP_T* udp_c);

/*!
 * Function name: udp_get_data_pointer
 * \return A pointer to the data part of an UDP frame.
 * \param udp_c : [in] udp_c of interest
 * \brief Give the data position in the frame to avoid copies.
 * cIPS has buffers for outgoing UDP frames. Building the frame consists
 * in filling the Ethernet, IP, UDP and then application data.
 * The (Ethernet, IP, UDP) section is constant. One solution consists
 * in copying the application data after the (Ethernet, IP, UDP) portion,
 * the other solution consists in keeping a reference to the data location.
 * Then, the application works on the reference.
 * \note Working on the reference has a limitation. The (Ethernet, IP, UDP)
 * size is a multiple of 32bits, therefore the udp_c->app_data reference is
 * not alligned on 32bits. If the application uses udp_get_data_pointer(),
 * then I advise to realigned the application data on 32bits.
 * *******************************************************************/
  u8_t* udp_get_data_pointer( UDP_T* udp_c );

/* UDP management. */

/*!
 * Function name: udp_parse
 * \return ERR_OK or ERR_CHECKSUM.
 * \param ip_frame : [in] IP frame.
 * \param ip_frame_length : Value of (IP header + UDP header + app_data).
 * \param net_adapter : [in] network adapter.
 * \brief The application opens some UDP ports (see udp_new()).
 * An UDP frame comes in, udp_parse() looks for the open port matching
 * the incoming frame.
 * *******************************************************************/
  err_t udp_parse(u8_t* ip_frame, u32_t ip_frame_length, struct NETIF_S *net_adapter);


#ifdef __cplusplus
}
#endif

#endif /* __UDP_H__ */


