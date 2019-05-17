#ident "@(#) $Id: netif.h,v 1.22 2011/11/29 16:33:53 jdavid Exp $"
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
#ifndef __NETIF_H__
#define __NETIF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "arch.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "tcp.h"

#define MAC_ADDRESS_LENGTH 6
#define UNUSED (~0) //!< Value indicating that a resource is not used
//! MTU_STORAGE: The Maximum Transfer Unit is the largest block of data that 
//! the network adapter exchange. The typical value is 1518. 
//! cIPS provides memory to store several of them.
//! cIPS runs on a 32 bit platform. The storage is optimized on a 32 bit
//! platform if each data is aligned to 4 bytes. Unfortunately, 1518 is not a
//! multiple of 4. So if cIPS stores two MTU in a row, the second MTU will not
//! be aligned. MTU_STORAGE fixes that. MTU_STORAGE alignes the MTU storage
//! on 4 bytes but cIPS does its logic on NETWORK_MTU bytes.
#define MTU_STORAGE (((NETWORK_MTU + sizeof(u32_t))/sizeof(u32_t))*sizeof(u32_t))

#define MAX_FORMATED_ERROR_SIZE 200 //!< Max size of formated_error.
//! Structure holding the last error.
typedef struct error_report_s
{
  err_t err_nb; //!<  error number
  s8_t formated_error[MAX_FORMATED_ERROR_SIZE]; //!< formated string
} ERROR_REPORT_T;

typedef struct NETIF_S {
  ERROR_REPORT_T last_error; //!<the last error.
  u32_t ip_addr;
  u32_t netmask;
  u32_t gateway_addr;
  u32_t subnetwork; //!<Prefix address defining the subnetwork and introduced to avoid recalculation. It is defined as (ip_addr & netmask).
  err_t (*ping_received)(struct NETIF_S *netif_ptr, void *arg, u8_t *icmp_data, u32_t icmp_data_length); //!<Callback when a ping is received.
  err_t (*ping_reply_received)(struct NETIF_S *netif_ptr, void *arg, u8_t *icmp_data, u32_t icmp_data_length); //!<Callback when a response to a ping is received.
  u32_t (*driver_recv)(void* pDriver_arg, u8_t *eth_frame); //!<The device driver link for reception (XEmacLite_Recv)
  err_t (*driver_send)(void* pDriver_arg, u8_t *eth_frame, u32_t byte_count); //!<The link to the device driver (XEmacLite_Send)
  void* pDriver_arg; //!< Backup of the first argument to use with "(*driver_send)".
  u8_t mac_address[MAC_ADDRESS_LENGTH];
  char name[3]; //!< last character in the end of string character.
  u32_t num; //!<  number of this interface or UNUSED if not used.
  ARP_CACHE_T arp_cache; //!< ARP resource

  //Receiving queue
  u8_t ethernet_frame_list[RECV_BUF_SIZE][MTU_STORAGE];//!<circular buffer of all ethernet frames received
  u8_t control_buffer[MTU_STORAGE];
  u32_t rcv_pos_insert; //!<index related to ethernet_frame_list only.
  u32_t rcv_pos_remove; //!<index related to filtered_ethernet_frame_list only.
  u32_t ISR_rcv_nb; //!<number of frames received in netif_ISR (and inserted into the circular buffer).
  u32_t processed_nb; //!<number of frames processed in netif_dispatch() (from the circular buffer).
  bool_t optimized; //!<flag indicating the level of filtering and whether UDP should be processed in the ISR.
  //Icmp arg
  void *callback_arg;
  //TCP and UDP receiving queues
  //!tcp_c_list[] holds all the tcp connectors. The tcp connectors can be servers, inherited from a server, clients or not used.
  //!tcp_server_cs is the start of a link-list. It links elements of tcp_c_list[].
  //!It links all the TCP controllers that are in a LISTEN state (i.e servers).
  //!The link list isolates a subset of connectors but also sorts them. As a result, cIPS goes fast through the connectors of interest.
  TCP_T *tcp_server_cs;
  TCP_T *tcp_active_cs; //!< tcp_active_cs has the same function as tcp_server_cs but for all the TCP controllers that are in a state in which they accept or send data (i.e inherited from a server and clients).The "active" name comes from the "active OPEN" transission in the RFC973.
  //!udp_c_list[] holds all the udp connectors. The udp connectors can be used or not.
  //!udp_cs is the start of a link-list. It links elements of udp_c_list[].
  //!It links all the UDP controllers that are used.
  UDP_T *udp_cs;
  TCP_T tcp_c_list[MAX_TCP]; //!< TCP resource (see comment above)
  UDP_T udp_c_list[MAX_UDP]; //!< UDP resource (see comment above)
  u8_t garbage_buffer[MTU_STORAGE]; //!< If netif cannot keep up with incoming frame interruption then they are put in this buffer and ignored
} NETIF_T;

/*!
 * Function name: netif_init
 * \return nothing
 * \brief Declare the network adapter as unused.
 * \brief cIPS works with static memory. cIPS has a finite list 
 * of MAX_NET_ADAPTER network adapters. netif_init() declares them
 * available.
 * *******************************************************************/
void netif_init(void);

/*!
 * Function name: netif_new
 * \return: a pointer to a network interface, NULL otherwise
 * \param mac_address : [in] MAC address
 * \param ipaddr : [in] ip address
 * \param netmask :[in] subnet
 * \param gateway_addr :[in] gateway addr 
 * (gateway of "0.0.0.0" or 0 means "no gateway")
 * \param name : [in] Name given to the adapter. Restricted to 2 letters.
 * \param optimized : [in] Flag. If TRUE, UDP are process in the Adapter ISR.
 * If FALSE, a filter is apply to avoid unwanted frames.
 * \param driver_recv : [in] Pointer of function to the device driver
 * receive function(XEmacLite_Recv).
 * \param driver_send : [in] interrupt ID related to the ethenet 
 * HW number "emac_deviceId"
 * \param pDriver_arg : [in] Device driver object (which is only 
 * stored in netif for reference).
 * \param err : [out] ERR_MEM if more than MAX_NET_ADAPTER already 
 * exist, ERR_OK otherwise.
 * \brief cIPS has a finite list of MAX_NET_ADAPTER network adapters.
 * netif_new() books one of them and initializes it.
 * \note The network interface is independant of the device driver.
 * The application creates the link between cIPs and the device driver
 * outside cIPS. The application takes the device driver API (driver_recv,
 * driver_send) and plugs it to cIPs through callbacks.
 * See network_adapter.c as an example.
 * *******************************************************************/
NETIF_T * netif_new( u8_t * mac_address, u32_t ipaddr, u32_t netmask,
      u32_t gw, const s8_t* const name, const bool_t optimized,
      u32_t (*driver_recv)(void* pDriver_arg, u8_t* FramePtr), //XEmacLite_Recv
      err_t (*driver_send)(void* pDriver_arg, u8_t* FramePtr, u32_t ByteCount), //XEmacLite_Send
      const void* pDriver_arg, err_t* err);

/*!
 * Function name: netif_delete
 * \return nothing
 * \param pnetif : [in] The network adapter of interest.
 * \brief cIPS has a finite list of MAX_NET_ADAPTER network adapters.
 * netif_delete() releases the one specified in argument.
 * *******************************************************************/
void netif_delete(NETIF_T *pnetif);

/*!
 * Function name: netif_ISR
 * \return nothing
 * \param network_adapter : [in] network adapter.
 * \brief The device driver receives an internet frames. It calls
 * netif_ISR(). netif_ISR() gives a pointer to the device driver.
 * The device driver fills the pointer with the incoming ethernet frame.
 * netif_ISR() stacks the ethernet frames in a list (or FIFO) of 
 * RECV_BUF_SIZE elements. Netif_dispatch() is in charge of emptying the
 * FIFO.
 * *******************************************************************/
void netif_ISR(void *network_adapter);

/*!
 * Function name: netif_ISR_optimized
 * \return nothing
 * \param network_adapter : [in] network adapter.
 * \brief Receives ethernet frames and processes UDP ones only.
 * This function is to be used in a context of a point to point 
 * connection with an UDP stream.
 * *******************************************************************/
void netif_ISR_optimized(void *network_adapter);

/*!
 * Function name: netif_dispatch
 * \return nothing
 * \param pnetif : [in] network adapter.
 * \brief This is the engine of the stack. It takes the frames stacked 
 * in a FIFO by netif_ISR(), parses them and dispatches them.
 * Parsing consists in identifing the protocol (IP or ARP) and 
 * forwarding the frame to the corresponding protocol layer.
 * *******************************************************************/
err_t netif_dispatch(NETIF_T *netif_ptr);

/*!
 * Function name: netif_send
 * \return ERR_OK or ERR_DEVICE_DRIVER
 * \param pnetif : [in] network adapter.
 * \param frame : [in] ethernet frame.
 * \param frame_length : [in] ethernet frame length.
 * \brief The ethernet frame is ready to be sent. cIPS forwards
 * the frame to the device driver. netif_send() is an encapsulation
 * of the device driver "send" function.
 * *******************************************************************/
err_t netif_send(NETIF_T* netif_ptr, u8_t* frame, u32_t frame_length);



/*!
 * Function name: netif_arg
 * \return nothing.
 * \param adapter : [out] adapter of interest.
 * \param arg : [in] argument structure.
 * \brief specify the argument that should be passed callback functions.
 * See netif_ping_received() and netif_ping_reply_received()
 * *******************************************************************/
void netif_arg (NETIF_T* adapter, void* arg);

/*!
 * Function name: netif_ping_received
 * \return nothing.
 * \param adapter : [out] adapter of interest.
 * \param ping_received : [in] Received callback.
 * \brief cIPS can notify the application of an incoming PING request.
 * To do so, the application registers a callback with cIPS by using 
 * netif_ping_received(). Upon a PING reception, cIPS calls the 
 * callback to notify the PING to the application.
 * *******************************************************************/
void netif_ping_received (NETIF_T* adapter, err_t (* ping_received)(NETIF_T* netif_ptr, void* arg, u8_t* icmp_data, u32_t icmp_data_length));

/*!
 * Function name: netif_ping_reply_received
 * \return nothing.
 * \param adapter : [out] adapter of interest.
 * \param ping_reply_received : [in] Received callback.
 * \brief The application can issue a ping request to a peer device.
 * The peer device replies to cIPS. The application would like cIPS to
 * notify it. To do so, the application registers a callback with
 * cIPS by using netif_ping_reply_received(). Upon a PING reply,
 * cIPS calls the callback to notify the PING reply to the application.
 * *******************************************************************/
void netif_ping_reply_received (NETIF_T *adapter, err_t (* ping_reply_received)(NETIF_T* netif_ptr, void* arg, u8_t *icmp_data, u32_t icmp_data_length));

/*!
 * Function name: netif_ip_route
 * \return the adpater associated to the IP address.
 * \param dest : [in] IP address whom adapter we are looking for.
 * \brief Finds the appropriate network interface for a given IP address. It
 * looks up the list of network interfaces linearly. A match is found
 * if the IP address of the network interface equals the 
 * IP address given to the function.
 * *******************************************************************/
NETIF_T* netif_ip_route(u32_t dest);

/*!
 * Function name: netif_filter
 * \return TRUE or FALSE
 * \param eth_frame : [in] ethernet frame.
 * \param pnetif : [in] network adapter.
 * \brief Accept IP, ARP, UDP, TCP and ICMP frames only.
 * *******************************************************************/
bool_t netif_filter(u8_t* eth_frame, NETIF_T* net_adapter);

/*!
 * Function name: get_last_stack_error
 * \return formated string with the error.
 * \param network_adapter : [in] network adapter sending the error.
 * \param err : [in] error number.
 * \brief netif_dispatch() is cIPS engine. It returns an error code as a long.
 *  get_last_stack_error() translates the error code in plain english.
 * *******************************************************************/
s8_t* get_last_stack_error( NETIF_T* const network_adapter, const err_t err);

/*!
 * Function name: adapter_store_error
 * \return nothing
 * \param err : [in] error number.
 * \param network_adapter : [in/out] network adapter sending the error.
 * \param function_name : [in] function name where the error is happening.
 * \param line_number : [in] line number where the error is happening.
 * \brief CIPS returns an error code which indicates the nature of the
 * error. But it is not enough. One would like to know the location 
 * of the error and its adapter (if several). adapter_store_error()
 * formats a string to report an error related to a network adapter.
 * *******************************************************************/
err_t adapter_store_error( const err_t err, NETIF_T* const network_adapter, const s8_t* const function_name, const u32_t line_number);

#ifdef __cplusplus
}
#endif

#endif /* __NETIF_H__ */
