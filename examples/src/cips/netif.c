#ident "@(#) $Id: netif.c,v 1.2 2013/12/04 20:50:42 jdavid Exp $"
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
* \namespace netif
* \file netif.c
* \brief Network Adapter description.
***************************************************/
#include "basic_c_types.h"
#include "ip.h"
#include "arp.h"
#include "udp.h"
#include "tcp.h"
#include "err.h"
#include "debug.h"
#include <stdio.h> //for sprintf
#include "netif.h"


NETIF_T g_MAC_adapter[MAX_NET_ADAPTER];  //!<Resource of abstract network adapters.


/*!
 * Function name: netif_init
 * \return nothing
 * \brief Declare the network adapter as unused.
 * \brief cIPS works with static memory. cIPS has a finite list 
 * of MAX_NET_ADAPTER network adapters. netif_init() declares them
 * available.
 * *******************************************************************/
void netif_init( void)
{
  u32_t i;
  
  for( i = 0; i < MAX_NET_ADAPTER; i++)
  {
    g_MAC_adapter[i].num = UNUSED;
  }

  (void)tcp_init();
}

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
 * \param err : [out] ERR_SEG_MEM if more than MAX_NET_ADAPTER already 
 * exist, ERR_OK otherwise.
 * \brief cIPS has a finite list of MAX_NET_ADAPTER network adapters.
 * netif_new() books one of them and initializes it.
 * \note The network interface is independant of the device driver.
 * The application creates the link between cIPs and the device driver
 * outside cIPS. The application takes the device driver API (driver_recv,
 * driver_send) and plugs it to cIPs through callbacks.
 * See network_adapter.c as an example.
 * *******************************************************************/
NETIF_T * netif_new(
      u8_t* mac_address,
      u32_t ipaddr,
      u32_t netmask,
      u32_t gateway_addr,
      const s8_t* const name,
      const bool_t optimized,
      u32_t (*driver_recv)(void* pDriver_arg, u8_t* FramePtr), //XEmacLite_recv
      err_t (*driver_send)(void* pDriver_arg, u8_t* FramePtr, u32_t ByteCount), //XEmacLite_Send
      const void* const pDriver_arg, err_t* err)
{
  u32_t i;
  NETIF_T *p = NULL;

  for( i = 0; i < MAX_NET_ADAPTER; i++)
  {
    if(g_MAC_adapter[i].num == (u32_t)UNUSED)
    {
      p = &g_MAC_adapter[i];
      p->num = i;
      i = MAX_NET_ADAPTER; //Exit loop
      *err = ERR_OK;
    }
  }
  if( !*err ) //!< Finish the construction.
  {
    p->ip_addr = ipaddr;
    p->netmask = netmask;
    p->gateway_addr = gateway_addr; //gateway of "0.0.0.0" or 0 means "no gateway"
    p->subnetwork = (p->ip_addr & p->netmask);//Prefix address defining the subnetwork and introduced to avoid recalculation.. It is defined as (ip_addr & netmask).
    //Protection against non valid gateway. A gateway must belong to the same subnet as the adapter IP adress.
    if( (p->gateway_addr) && ((p->gateway_addr & p->netmask) != (p->ip_addr & p->netmask)))
    {
      //p->gateway_addr is likely to be wrong but does not prevent the app to run if the gateway is not used.
      p->gateway_addr = (p->ip_addr & p->netmask) | (p->gateway_addr & ~(p->netmask));
      T_ERROR(("Wrong gateway address %s#%ld\r\n",__func__, __LINE__));
    }
    p->ping_received = NULL;
    p->ping_reply_received = NULL;
    p->driver_recv = driver_recv;
    p->driver_send = driver_send;
    p->pDriver_arg = (void*)pDriver_arg;
    for( i = 0; i < MAC_ADDRESS_LENGTH; i++)
    { p->mac_address[i] = mac_address[i];}
    p->name[0] = name[0];
    p->name[1] = name[1];
    p->name[2] = 0x00;  /*!< The name is a NULL terminated string. */
    (void)arp_init_cache(&(p->arp_cache));
    {
      ETHER_HEADER_T * ethernet_header;
      for( i= 0; i < RECV_BUF_SIZE; i++)
      {
        ethernet_header = (ETHER_HEADER_T*)(p->ethernet_frame_list[p->rcv_pos_remove]);
        ethernet_header->frame_type = 0; //This is acting as a reset of the frame
      }
    }
    p->rcv_pos_insert = 0;
    p->rcv_pos_remove = 0;
    p->ISR_rcv_nb = 0;
    p->ISR_rcv_nb = p->processed_nb;
    p->optimized = optimized;
    p->tcp_server_cs = NULL;  /*!< List of all TCP controllers that are in a LISTEN state. */
    p->tcp_active_cs = NULL;  /*!< List of all TCP controllers that are in a state in which they accept or send data. */
    p->udp_cs = NULL;  /*!< List of all UDP controllers. */
    p->callback_arg = NULL;
    for( i = 0; i < MAX_TCP; i++)
    {
      p->tcp_c_list[i].id = UNUSED;
    }

    for( i = 0; i < MAX_UDP; i++)
    {
      p->udp_c_list[i].state = UNUSED;
    }
  } else {
    *err = ERR_SEG_MEM;
  }
  return p;
}

/*!
 * Function name: netif_delete
 * \return nothing
 * \param pnetif : [in] The network adapter of interest.
 * \brief cIPS has a finite list of MAX_NET_ADAPTER network adapters.
 * netif_delete() releases the one specified in argument.
 * *******************************************************************/
void netif_delete(NETIF_T *pnetif)
{
  pnetif->num = UNUSED; //free the resource so it can be reused by "netif_new"
  pnetif->netmask = 0; //Reject all the incoming frames in "netif_filter"
  pnetif->driver_send = NULL; // Shortcut "netif_send"
  return;
}

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
err_t netif_send(NETIF_T *pnetif, u8_t *frame, u32_t frame_length)
{
  err_t err = ERR_OK;

  if( pnetif->driver_send )
  {
    pnetif->driver_send(pnetif->pDriver_arg, frame, frame_length);
    if(err)
    {
      err = adapter_store_error( ERR_DEVICE_DRIVER, pnetif, __func__, __LINE__);
    }
  }
  return err;
}

/*!
 * Function name: netif_filter
 * \return TRUE or FALSE
 * \param eth_frame : [in] ethernet frame.
 * \param pnetif : [in] network adapter.
 * \brief Accept IP, ARP, UDP, TCP and ICMP frames only.
 * *******************************************************************/
bool_t netif_filter(u8_t* eth_frame, NETIF_T *pnetif)
{
  ETHER_HEADER_T * ethernet_header;
  bool_t accepted = FALSE;

  ethernet_header = (ETHER_HEADER_T*)eth_frame;

  if( ethernet_header->frame_type == htons(ETHERTYPE_IP) ) {
    u8_t protocol;
    IP_HEADER_T * ip_header;
    ip_header = (IP_HEADER_T*)(eth_frame + sizeof(ETHER_HEADER_T));
    protocol = ip_header->protocol;

    if( (protocol == IP_UDP) || (protocol == IP_TCP) || (protocol == IP_ICMP)) {
      if(((ntohl(ip_header->dest_addr)) & pnetif->netmask) == pnetif->subnetwork) {//discard what does not belong to the sub-network defined by netmask
        accepted = TRUE;
      } else {
        T_DEBUGF(NETIF_DEBUG, ("%s: IP@ out of subnetwork\r\n",pnetif->name));}
    }
  } else if( ethernet_header->frame_type == htons(ETHERTYPE_ARP)) {
    ARP_HEADER_T* arp_header;
    arp_header = (ARP_HEADER_T*)(eth_frame + sizeof(ETHER_HEADER_T));

    if( ntohl(arp_header->target_ip_addr) == pnetif->ip_addr ) { 
      accepted = TRUE;
    }
  }

  return accepted;
}


/*!
 * Function name: netif_ip_route
 * \return the adpater associated to the IP address.
 * \param dest : [in] IP address whom adapter we are looking for.
 * \brief Finds the appropriate network interface for a given IP address. It
 * looks up the list of network interfaces linearly. A match is found
 * if the IP address of the network interface equals the 
 * IP address given to the function.
 * *******************************************************************/
NETIF_T* netif_ip_route(u32_t dest)
{
  u32_t i;
  NETIF_T *p = NULL;
  
  for( i = 0; i < MAX_NET_ADAPTER; i++)
  {
    if ((g_MAC_adapter[i].num != (u32_t)UNUSED) && (dest == g_MAC_adapter[i].ip_addr))
    {
      p = &g_MAC_adapter[i];
      i = MAX_NET_ADAPTER; //Exit loop
    }
  }

  return p;
}

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
void netif_ISR(void *network_adapter)
{
  NETIF_T* pnetif = (NETIF_T*) network_adapter;

  if (pnetif->ISR_rcv_nb != pnetif->processed_nb + RECV_BUF_SIZE) //circular buffer not full
  {
    //Prepare the next indexes in case a new interruption occurs as soon as "pnetif->driver_recv" returns
    u32_t frame_length;
    u32_t rcv_pos_insert;
    u8_t* rcv_buf;

    rcv_buf = pnetif->ethernet_frame_list[pnetif->rcv_pos_insert];
    rcv_pos_insert = ( pnetif->rcv_pos_insert != RECV_BUF_SIZE - 1 )? (pnetif->rcv_pos_insert+1): 0; //index of the next ISR

    //Get the frame from the device driver. Due to the device driver lock, every instruction from the beginning of this function to here is in a "critical section".
    frame_length = pnetif->driver_recv(pnetif->pDriver_arg, rcv_buf);

    if ( frame_length )
    { //Filter the frame
      bool_t accepted;
      accepted = netif_filter(rcv_buf, pnetif);
      if( accepted)
      { //Enqueue the frame
        pnetif->rcv_pos_insert = rcv_pos_insert;
        pnetif->ISR_rcv_nb++;
      }
    }
  }
  else
  { //The FIFO is full: the processing (via netif_dispatch() ) cannot keep up 
    //with the amount of ISR coming in.
    //Increase RECV_BUF_SIZE or the frame is discarded
    (void)pnetif->driver_recv(pnetif->pDriver_arg, pnetif->garbage_buffer);
    T_ERROR(("Too many incoming frames %s#%ld\r\n",__func__, __LINE__));
  }
  return;
}

/*!
 * Function name: netif_ISR_optimized
 * \return nothing
 * \param network_adapter : [in] network adapter.
 * \brief Receives ethernet frames and processes UDP ones only.
 * This function is to be used in a context of a point to point 
 * connection with an UDP stream.
 * *******************************************************************/
void netif_ISR_optimized(void *network_adapter)
{
  NETIF_T * pnetif = (NETIF_T *) network_adapter;

  if (pnetif->ISR_rcv_nb != pnetif->processed_nb + RECV_BUF_SIZE)
  {
    //Prepare the next indexes in case a new interruption occurs as soon as "pnetif->driver_recv" returns
    u32_t frame_length;
    u32_t rcv_pos_insert;
    u8_t* rcv_buf;

    rcv_buf = pnetif->ethernet_frame_list[pnetif->rcv_pos_insert];
    rcv_pos_insert = ( pnetif->rcv_pos_insert != RECV_BUF_SIZE - 1 )? (pnetif->rcv_pos_insert+1): 0; //index of the next ISR

    //Get the frame from the device driver. Due to the device driver lock, every instruction from the beginning of this function to here is in a "critical section".
    frame_length = pnetif->driver_recv(pnetif->pDriver_arg, rcv_buf);
    if ( frame_length )
    {
      IP_HEADER_T * ip_header;
      ETHER_HEADER_T * ethernet_header;
      u8_t* frame;
      frame = rcv_buf;
      //Put an UDP filter
      ethernet_header = (ETHER_HEADER_T*)frame;
      ip_header = (IP_HEADER_T*)(frame + sizeof(ETHER_HEADER_T));
      if( (ethernet_header->frame_type == ntohs(ETHERTYPE_IP)) && (ip_header->protocol == IP_UDP))
      {  //Forward directly to the UDP parser
        (void)udp_parse((u8_t*)ip_header, (u32_t)ntohs(ip_header->length), pnetif);
      }
      else if( netif_filter(rcv_buf, pnetif)) //Filter the frame
      { //Enqueue the frame
        pnetif->rcv_pos_insert = rcv_pos_insert;
        pnetif->ISR_rcv_nb++;
      }
    }
  }
  else
  { //The FIFO is full: the processing (via netif_dispatch() ) cannot keep up 
    //with the amount of ISR coming in.
    //Increase RECV_BUF_SIZE or the frame is discarded
    (void)pnetif->driver_recv(pnetif->pDriver_arg, pnetif->garbage_buffer);
    T_ERROR(("Too many incoming frames %s#%ld\r\n",__func__, __LINE__));
  }

  return;
}

/*!
 * Function name: netif_dispatch
 * \return nothing
 * \param pnetif : [in] network adapter.
 * \brief This is the engine of the stack. It takes the frames stacked 
 * in a FIFO by netif_ISR(), parses them and dispatches them.
 * Parsing consists in identifing the protocol (IP or ARP) and 
 * forwarding the frame to the corresponding protocol layer.
 * *******************************************************************/
err_t netif_dispatch(NETIF_T *pnetif)
{
  err_t err = ERR_OK;

  if ( pnetif->processed_nb != pnetif->ISR_rcv_nb )
  {
    ETHER_HEADER_T * ethernet_header;
    u32_t frame_length;
    u8_t* eth_frame;

    eth_frame = pnetif->ethernet_frame_list[pnetif->rcv_pos_remove];
    ethernet_header = (ETHER_HEADER_T *)eth_frame;

    if( ethernet_header->frame_type == ntohs(ETHERTYPE_IP) ) { //IPv4
      ETHER_IP_HEADER_T* ethernet_ip_header = (ETHER_IP_HEADER_T*)eth_frame;
      frame_length = (u32_t)ntohs(ethernet_ip_header->ip.length) + sizeof(ETHER_HEADER_T);
      if( frame_length > NETWORK_MTU - (sizeof(ETHER_HEADER_T) + ETHER_CRC_LENGTH)) { //Max size of an IP frame. That situation is unlikely to happen but still can. The max will safely limit the checksum scope of calculation and then reject that improper frame.
        frame_length = NETWORK_MTU - (sizeof(ETHER_HEADER_T) + ETHER_CRC_LENGTH);
      }
      err = ip_parse(eth_frame, frame_length, pnetif);
    } else if (ethernet_header->frame_type == ntohs(ETHERTYPE_ARP) ) {
      err = arp_parse(eth_frame, pnetif);
    }

    ethernet_header->frame_type = 0; //This is acting as a reset of the frame when the parsing is done.
    pnetif->rcv_pos_remove = ( pnetif->rcv_pos_remove != RECV_BUF_SIZE - 1 )? (pnetif->rcv_pos_remove+1): 0; //next index
    pnetif->processed_nb++;
  }

  return err;
}

/*!
 * Function name: netif_arg
 * \return nothing.
 * \param adapter : [out] adapter of interest.
 * \param arg : [in] argument structure.
 * \brief specify the argument that should be passed callback functions.
 * See netif_ping_received() and netif_ping_reply_received()
 * *******************************************************************/
void netif_arg(NETIF_T *adapter, void *arg)
{
  adapter->callback_arg = arg;
}

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
void netif_ping_received (NETIF_T *adapter, err_t (* ping_received)(NETIF_T *pnetif, void *arg, u8_t *icmp_data, u32_t icmp_data_length))
{
  adapter->ping_received = ping_received;
}

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
void netif_ping_reply_received (NETIF_T *adapter, err_t (* ping_reply_received)(NETIF_T *pnetif, void *arg, u8_t *icmp_data, u32_t icmp_data_length))
{
  adapter->ping_reply_received = ping_reply_received;
}

/*!
 * Function name: get_last_stack_error
 * \return formated string with the error.
 * \param network_adapter : [in] network adapter sending the error.
 * \param err : [in] error number.
 * \brief netif_dispatch() is cIPS engine. It returns an error code as a long.
 *  get_last_stack_error() translates the error code in plain english.
 * *******************************************************************/
s8_t* get_last_stack_error( NETIF_T* const network_adapter, const err_t err)
{
  ERROR_REPORT_T* last_error = &(network_adapter->last_error);
  if ( err != last_error->err_nb)
  {  snprintf(last_error->formated_error,MAX_FORMATED_ERROR_SIZE,"Error: %s ", decode_err(err));}

  return last_error->formated_error;
}

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
err_t adapter_store_error( const err_t err, NETIF_T* const network_adapter, const s8_t* const function_name, const u32_t line_number)
{
  ERROR_REPORT_T* last_error = &(network_adapter->last_error);
  last_error->err_nb = err;
  snprintf(last_error->formated_error,MAX_FORMATED_ERROR_SIZE,"%s: Error: %s (fct %s #%ld)",network_adapter->name, decode_err(err), function_name, line_number);
  return err;
}

