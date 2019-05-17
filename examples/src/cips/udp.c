#ident "@(#) $Id: udp.c,v 1.2 2013/12/04 20:50:43 jdavid Exp $"
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
* \namespace udp
* \file udp.c
* \brief  Module Description: UDP stack.
***************************************************/

#include <stdio.h> //for sprintf
#include "basic_c_types.h"
#include "nw_protocols.h"
#include "arp.h"
#include "ip.h"
#include "netif.h"
#include "err.h"
#include "debug.h"
#include "udp.h"

#define ETH_IP_UDP_HEADER_SIZE ( sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(UDP_HEADER_T) )

/*!UDP states
UDP_UNUSED: the udp_c is free.
UDP_KNOWN_TARGET: the udp_c is created and can receive frames only from the socket defined by udp_connect. It can be seen as a client
UDP_ANY_TARGET: the udp_c is created and can receive frames from any other UDP socket. It can be seen as a server*/
typedef enum {
  UDP_KNOWN_TARGET = 0,
  UDP_ANY_TARGET = 1,
  UDP_UNUSED = (u32_t)UNUSED
} UDP_STATE_T;

static err_t udp_store_error( const err_t err, UDP_T* const udp_c, const s8_t* const function_name, const u32_t line_number);
static u16_t udp_new_port( NETIF_T* net_adapter);
static UDP_T* udp_malloc(NETIF_T *net_adapter);
static void udp_remove_controller( UDP_T** udp_cs,  UDP_T* udp_c );
static void udp_register( UDP_T** udp_cs,  UDP_T* udp_c );
static void udp_order_active_list( UDP_T** udp_cs, const NETIF_T* const net_adapter);
static void udp_init_connection (UDP_T* const udp_c, const u8_t* const dest_mac_addr);


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
err_t udp_parse(u8_t* ip_frame, u32_t ip_frame_length, NETIF_T *net_adapter)
{
  UDP_T *udp_c;
  UDP_HEADER_T* udphdr;
  IP_HEADER_T* iphdr;
  u32_t ip_header_length;
  err_t err = ERR_OK;

  iphdr = (IP_HEADER_T*) ip_frame; 
  ip_header_length = IP_GET_HEADER_LENGTH(iphdr);
  udphdr = (UDP_HEADER_T *)(ip_frame + ip_header_length);

  //Look for the "udp_c" which port matches the incoming frame port.
  udp_c = net_adapter->udp_cs;
  while((udp_c != NULL) && !( udp_c->local_port == ntohs(udphdr->dest_port)) )
  {
    udp_c = udp_c->next;
  }

  if (udp_c != NULL ) //A candidate has been found.
  {
    u32_t checksum;
    u32_t udp_length;
    u16_t pseudo_checksum;

    //Verify UDP checksum.
    if( udphdr->chksum )
    {
      udp_length = ip_frame_length - ip_header_length;
      pseudo_checksum = eth_build_pseudo_header( ntohl(iphdr->dest_addr) , ntohl(iphdr->source_addr), udp_length - sizeof(UDP_HEADER_T), sizeof(UDP_HEADER_T), IP_UDP);
      if(pseudo_checksum != udphdr->chksum){
        checksum = ip_checksum( (const u16_t*)udphdr, udp_length);
        checksum += (u32_t)pseudo_checksum;
        //Fold 32-bit sum to 16 bits and add carry
        while( checksum >> 16) {
          checksum = (checksum & 0xFFFF) + (checksum >> 16);
        }
      }
      else
      {
        //The peer has delegated the checksum to the adapter but the adapter has not calculated it (when it should had!).
        //As a result the frame only has its pseudo checksum. Wireshark displays "maybe caused by "TCP chcksum offload").
        checksum = CHECKSUM_OK;
      }
    }
    else
    { checksum = CHECKSUM_OK;} //No checksum

   if( checksum == CHECKSUM_OK)
   {
      //Keep details of the incoming frame in case of a reply in "udp_c->recv". This case is for UDP servers.
      if( ( udp_c->state == UDP_ANY_TARGET) && ((udp_c->remote_port != ntohs(udphdr->source_port)) || (udp_c->remote_ip != ntohl(iphdr->source_addr)))){
        int i = 0;
        //Backup field to be ready to reply to this incoming frame
        ETHER_HEADER_T* ethhdr = (ETHER_HEADER_T*) (ip_frame - sizeof(ETHER_HEADER_T));
        do {
          udp_c->target_mac_addr[i] = ethhdr->source_addr[i];
        } while ( ++i < MAC_ADDRESS_LENGTH);
        udp_c->remote_ip = ntohl(iphdr->source_addr);
        udp_c->remote_port = ntohs(udphdr->source_port);
        udp_c->frame_initialized = FALSE;
      }

      T_DEBUGF(UDP_DEBUG,("%s#%d:UDP: recv %d bytes from port #%d\r\n",udp_c->netif->name, ntohs(udphdr->dest_port), ntohs(udphdr->length), ntohs(udphdr->source_port)));
      if( (udp_c->recv != NULL) && 
          (udp_c->remote_ip == ntohl(iphdr->source_addr)) && (udp_c->remote_port = ntohs(udphdr->source_port)) //Security: These conditions prevent a Client to reach a Client. i.e. a connection NOT agreed with udp_connect().
        )
      { err = udp_c->recv(udp_c->recv_arg, udp_c, ip_frame + ip_header_length + sizeof(UDP_HEADER_T), (u32_t)ntohs(udphdr->length) - sizeof(UDP_HEADER_T));}
    }
    else
    {
      T_DEBUGF(UDP_DEBUG,("%s#%d:UDP: checksum error, frame dropped\r\n",udp_c->netif->name, ntohs(udphdr->dest_port)));
      err =adapter_store_error( ERR_CHECKSUM, net_adapter, __func__, __LINE__);
    }
  }
  return err;
}

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
void udp_recv (UDP_T *udp_c, err_t (* recv)(void *arg, UDP_T *udp_c, void* data, u32_t data_length), void *recv_arg)
{
  //cIPS remembers the application's callback and the data associated to the callback argument.
  udp_c->recv_arg = recv_arg;
  udp_c->recv = recv;
}
/*!
 * Function name: udp_delete
 * \return nothing.
 * \param udp_c: [in/out] connection of interest.
 * \brief udp_delete() is the opposite of udp_new(). udp_new() allocates
 * the resource and configures it (IP address, port#...). udp_delete()
 * frees the resource. It implies that the resource can be reused.
 * *******************************************************************/
void udp_delete(UDP_T *udp_c)
{
  udp_c->state = UDP_UNUSED;
 (void) udp_remove_controller( &(udp_c->netif->udp_cs), udp_c );
}

/*!
 * Function name: udp_new
 * \return the udp_c created or NULL if no more controllers can be created.
 * \param ipaddr: [in] IP address linked to that udp_c.
 * \param port: [in] Port number linked to that udp_c.
 * \param point_to_point: [in] Flag. If TRUE, the link is point to point and no 
 * UDP checksum is processed. The ethernet and IP checksums are enough 
 * to guaranty the integrity of the frame.
 * \param err: [out]ERR_UDP_MEM if more than MAX_UDP udp connection already exist,
 * ERR_VAL if the IP@ is wrong, ERR_OK otherwise
 * \brief Create a new UDP Protocol Control Block.
 * *******************************************************************/
UDP_T* udp_new( u32_t ipaddr, u16_t port, bool_t point_to_point, err_t* err)
{
  UDP_T *udp_c = NULL;
  NETIF_T *net_adapter;

  net_adapter = netif_ip_route(ipaddr);
  if( net_adapter != NULL )
  {
    udp_c = udp_malloc( net_adapter);
    if( udp_c != NULL)
    {
      udp_c->point_to_point = point_to_point; //<! Flag. If TRUE, the link is point to point and no checksum is processed. The ethernet checksum is enough to guaranty the integrity of the frame.
      udp_c->local_ip = ipaddr;
      udp_c->local_port = (port != 0)?port:udp_new_port( net_adapter );
      T_DEBUGF(UDP_DEBUG,("%s#%d: bound to port %d\r\n",net_adapter->name, udp_c->local_port, udp_c->local_port));
      (void) udp_register( &(net_adapter->udp_cs), udp_c );
      *err = ERR_OK;
    }
    else
    {
      T_ERROR(("Cannot create new UDP controller. Increase max_udp_c (>%d)\r\n",MAX_UDP));
      *err = ERR_UDP_MEM;
    }
  }
  else
  {
    T_ERROR(("%s : Cannot find the adapter linked to the IP address %ld.%ld.%ld.%ld\r\n",net_adapter->name, (ipaddr >> 24) & 0xff, (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff));
    *err = ERR_VAL;
  }
  return udp_c;
}

/*!
 * Function name: udp_connect
 * \return ERR_OK, ERR_VAL, ERR_MAC_ADDR_UNKNOWN
 * \param udp_c : [in/out] udp_c of interest.
 * \param ipaddr : [in] remote IP address to reach.
 * \param port : [in] remote port to reach.
 * \brief Connect a remote socket.
 * \note The application wants to reach a peer device. But cIPS does not
 * know the peer device's MAC address. The application calls
 * udp_connect() in order to resolve the MAC addres.
 * The MAC address can be in the ARP table or can be resolve
 * by an ARP request.
 * *******************************************************************/
err_t udp_connect(UDP_T* udp_c, u32_t ipaddr, u16_t port)
{
  err_t err = ERR_OK;

  //Connect only once after creation (i.e UDP_ANY_TARGET state). "udp_connect" can be called many times but the change of state will be done only once in the life of the socket.
  if( udp_c->state == UDP_ANY_TARGET) {
    u32_t dest_ip_or_gateway;
    T_ASSERT(("%s#%d Invalid IP@.\r\n",__func__, __LINE__), ipaddr != 0);
    T_ASSERT(("%s#%d Invalid port.\r\n",__func__, __LINE__), port != 0);

    dest_ip_or_gateway = arp_get_route(ipaddr, udp_c->netif->gateway_addr, udp_c->netif->netmask);

    //Check that the peer device is reachable.
    if (arp_query_cache(&(udp_c->netif->arp_cache), dest_ip_or_gateway, udp_c->target_mac_addr, udp_c->netif->netmask) != ERR_VAL)
    {
      udp_c->remote_ip = ipaddr;
      udp_c->remote_port = port;
      udp_c->state = UDP_KNOWN_TARGET;
      T_DEBUGF(UDP_DEBUG, ("udp_connect: connected to %ld.%ld.%ld.%ld, port %d\r\n",
                 (udp_c->remote_ip >> 24 & 0xff), (udp_c->remote_ip >> 16 & 0xff), (udp_c->remote_ip >> 8 & 0xff), (udp_c->remote_ip & 0xff), udp_c->remote_port));
      (void)udp_register(&(udp_c->netif->udp_cs), udp_c);
    } else { //The destination MAC address is unknow, send an ARP request to resolve it.
      u32_t framelen =  eth_build_frame( udp_c->target_mac_addr, udp_c->netif->mac_address, dest_ip_or_gateway, udp_c->local_ip, udp_c->netif->control_buffer, ETH_ARP_REQUEST);
      (void)netif_send(udp_c->netif, udp_c->netif->control_buffer, framelen);
      err = udp_store_error( ERR_MAC_ADDR_UNKNOWN, udp_c, __func__, __LINE__);
    }
  }
  return err;
}


/*!
 * Function name: udp_send
 * \return ERR_OK, ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param udp_c : [in] Descriptor block containing the buffer to send (udp_c->frame).
 * \param data : [in] Application data. If set to NULL then udp_c->app_data constitues the data and no copies in performed.
 * \param data_length : [in] Application data length in bytes.
 * \param reuse : [in] TRUE if the size of data and the dest does not change. It reuses most of the previous frame.
 * \brief The application uses udp_send() in order to send an ethernet 
 * frame (udp_c->frame) to the network. ( udp_c->frame includes the app data udp_c->app_data).
 * \note udp_send_to is an encapsulation of udp_new, udp_connect, udp_send and udp_close
 * *******************************************************************/
err_t udp_send(UDP_T *udp_c, void* data, u32_t data_length, bool_t reuse)
{
  u32_t length;
  UDP_HEADER_T *udphdr;
  err_t err = ERR_OK;

  T_ASSERT(("%s#%d UDP socket is not created.\r\n",__func__, __LINE__), udp_c != NULL);
  T_ASSERT(("%s#%d Pointer udp_c->app_data moved. It must be constant.\r\n",__func__, __LINE__), udp_c->app_data == udp_c->frame + sizeof(UDP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));

  if(udp_c->remote_ip ) { //Means that it is not in the UDP_UNUSED state and that cIPS knows everything about the target to communicate with
    length = sizeof(UDP_HEADER_T) + data_length;

    //Fill in UDP part
    udphdr = (UDP_HEADER_T *)(udp_c->frame + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));

    if( data != NULL) {//if ( data == NULL) then data are inserted directly into the frame as they are gathered. "udp_get_data_pointer" is used.
      //Insert data into the frame
      u8_t* psrc = (u8_t*)data;
      u8_t* pdst = udp_c->app_data;
      u32_t i = data_length;
      while(i--) {
        *(pdst++) = *(psrc++);
      }
    }
    //cIPs limits the calculation. 
    //Some application only sends the same structure of application data to a peer device.
    //The structure has a fixed length. If the length is constant and the peer device always the same
    //then cIPS does not need to update a few fields. The fields in the following case:
    if( (!reuse) || (!udp_c->frame_initialized)) { //constant length: do not update the length and speudo-cheksum.
      udphdr->length = htons(length);
      if( !udp_c->point_to_point ) {
        udp_c->chksum_len = eth_build_pseudo_header( udp_c->remote_ip , udp_c->local_ip, data_length, sizeof(UDP_HEADER_T), IP_UDP);
      } else {
        udphdr->chksum = 0; // "udphdr->chksum" stays at zero if there is no checksum required (point to point case). Otherwise, cIPS resets it before calculating the checksum ip_checksum().
      }
      (void)eth_build_ip_request(udp_c->remote_ip, udp_c->local_ip, udp_c->frame + sizeof(ETHER_HEADER_T),data_length + sizeof(UDP_HEADER_T), IP_UDP, udp_c->frame_initialized);
      (void)udp_init_connection (udp_c, udp_c->target_mac_addr);
    }

    //Another optimization. If the connection to the peer device is point-to-point
    //many checksums certifiy the frame (MAC, IP, UDP). It is not true anymore if the frame goes
    //through a gateway. The following case saves the UDP checksum calculation
    if( !udp_c->point_to_point ) {
      u32_t checksum;
      udphdr->chksum = 0; //The checksum calculation ip_checksum() adds "udphdr->chksum" and requires that it is set to zero.
      checksum = udp_c->chksum_len; //"checksum" is big endian. "udp_c->chksum_len" is big endian because it is the return of eth_build_pseudo_header() which is big endian.
      checksum += ip_checksum((const u16_t*)udphdr, length); // ones complement cksum of struct
      //Fold 32-bit sum to 16 bits and add carry
      while( checksum >> 16) {
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
      }
      udphdr->chksum = (~((u16_t)checksum)); //checksum is the one's complement of the calculated ckecksum. Note: checksum and tcphdr->chksum are big endian.
      if( udphdr->chksum == 0 ) {// chksum zero must become 0xffff, as zero means 'no checksum' 
        udphdr->chksum = (u16_t)0xFFFF;
      }
    }

    T_DEBUGF(UDP_DEBUG,("%s#%d:UDP: send %d bytes to remote port #%d\r\n",udp_c->netif->name, udp_c->local_port, ntohs(udphdr->length), ntohs(udphdr->dest_port)));
    err = netif_send(udp_c->netif,udp_c->frame, ETH_IP_UDP_HEADER_SIZE + data_length);
  }

  return err;
}

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
u8_t* udp_get_data_pointer( UDP_T* udp_c )
{
  return udp_c->app_data;
}

/*!
 * Function name: udp_store_error
 * \return nothing
 * \param err : [in] error number.
 * \param udp_c : [in/out] udp_c sending the error.
 * \param function_name : [in] function name where the error is happening.
 * \param line_number : [in] line number where the error is happening.
 * \brief Format a string to report a UDP error.
 * *******************************************************************/
static err_t udp_store_error( const err_t err, UDP_T* const udp_c, const s8_t* const function_name, const u32_t line_number)
{
  ERROR_REPORT_T* last_error = &(udp_c->netif->last_error);
  last_error->err_nb = err;
  snprintf(last_error->formated_error,MAX_FORMATED_ERROR_SIZE,"%s#%d-%d: UDP error: %s (fct %s #%ld)",udp_c->netif->name, udp_c->local_port, udp_c->remote_port, decode_err(err), function_name, line_number);
  return err;
}


/*!
 * Function name: udp_malloc
 * \return free_udp_c: a pointer to a new udp_c.
 * \param net_adapter : [in] network adapter.
 * \brief cIPS works with static memory. cIPS has a finite list (or array)
 * of MAX_UDP UDP connection. udp_malloc() books a udp_c in the list 
 * of UDP controllers and returns a
 * reference to it.
 * *******************************************************************/
static UDP_T* udp_malloc(NETIF_T *net_adapter)
{
  UDP_T* free_udp_c = NULL;
  UDP_T* udp_c_list = net_adapter->udp_c_list;
  u32_t i;

  for( i = 0; i < MAX_UDP; i++)
  {
    if( udp_c_list[i].state == UDP_UNUSED)
    { //Configure the new connection
      free_udp_c = &udp_c_list[i];
      free_udp_c->local_ip = 0;
      free_udp_c->remote_ip = 0;
      free_udp_c->next = NULL;
      free_udp_c->local_port = 0;
      free_udp_c->remote_port = 0;
      free_udp_c->chksum_len = 0;
      free_udp_c->app_data = free_udp_c->frame + sizeof(UDP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T);
      free_udp_c->frame_initialized = FALSE;
      free_udp_c->netif = net_adapter;
      free_udp_c->state = UDP_ANY_TARGET;
      free_udp_c->recv = NULL;
      free_udp_c->recv_arg = NULL;
      i = MAX_UDP; // exit loop
    }
  }

  return free_udp_c;
}

/*!
 * Function name: udp_order_active_list
 * \return nothing
 * \param udp_cs : [out] List of active UDP connections.
 * \param net_adapter : [in] network adapter.
 * \brief When an ethernet frame comes, cIPS tries to match it with an
 * existing connection. The match is done by looking up the UDP list.
 * It is more efficient if the list is small and ordered. 
 * udp_order_active_list() re-order the list of UDP connections. It
 * puts the open connection at the beginning of the list
 * *******************************************************************/
static void udp_order_active_list( UDP_T** udp_cs, const NETIF_T* const net_adapter)
{
#ifdef UDP_DEBUG
  u32_t unused, client, server;
#endif //UDP_DEBUG
  UDP_T *udp_c_i;
  u32_t i;
  u32_t prec;
  UDP_T* udp_c_list = (UDP_T*)net_adapter->udp_c_list;

  //look for first elt.
  prec = UDP_UNUSED;
  for( i = 0; i < MAX_UDP; i++)
  {
    udp_c_i = &(udp_c_list[i]);
    if( udp_c_i->state != UDP_UNUSED)
    {
      prec = i; //current elt becomes precedent elt.
      i = MAX_UDP; // exit loop
    }
  }

  //Set the "active list" to the first elt.
  *udp_cs = (prec != UDP_UNUSED) ? &(udp_c_list[prec]) : NULL;

  //build active connection list.
  for( i = prec+1; i < MAX_UDP; i++)
  {
    udp_c_i = &(udp_c_list[i]);
    if( (udp_c_i->state != UDP_UNUSED) )
    {
      udp_c_list[prec].next = udp_c_i;
      prec = i; //current elt becomes precedent elt.
    }
  }
  //last elt
  udp_c_list[prec].next = NULL;

#ifdef UDP_DEBUG
  unused =0;
  client =0;
  server=0;

  for( i = 0; i < MAX_UDP; i++)
  {
    udp_c_i = &(udp_c_list[i]);
    switch( udp_c_i->state)
    {
      case UDP_UNUSED:
        unused++;
      break;
      case UDP_KNOWN_TARGET:
        client++;
      break;
      case UDP_ANY_TARGET:
        server++;
      break;
    }
  }
    T_DEBUGF(UDP_DEBUG, ("UDP controllers: unused=%ld client=%ld server=%ld\r\n",unused, client, server));
#endif //UDP_DEBUG
  return;
}

/*!
 * Function name: udp_register
 * \return nothing
 * \param udp_cs : [out] List of active UDP controllers.
 * \param udp_c : [in] udp_c of interest, udp_c->state must be set to a different value of
 * UNUSED to register the udp_c.
 * \brief Add the new connection to the list of active UDP connections.
 * It also rebuilds the list.
 * *******************************************************************/
static void udp_register( UDP_T** udp_cs, UDP_T* udp_c )
{
  (void) udp_order_active_list( udp_cs, udp_c->netif);
}

/*!
 * Function name: udp_remove_controller
 * \return nothing
 * \param udp_cs : [out] List of active UDP controllers.
 * \param udp_c : [in] udp_c of interest.
 * \brief Remove a connection to the list of UDP connections.
 * It also rebuilds the list.
 * *******************************************************************/
static void udp_remove_controller( UDP_T** udp_cs,  UDP_T* udp_c )
{
  (void) udp_order_active_list( udp_cs, udp_c->netif);
}

/*!
 * Function name: udp_new_port
 * \return new port number
 * \param net_adapter : [in/out] udp_c of interest.
 * \brief The application creates a UDP connection. It assigns a port
 * number or let cIPS assign one. I cIPS assignes one, it calls\
 * udp_new_port(). udp_new_port() looks for a free port.
 * \note : Return the highest port number (or the oldest allocated) 
 * if no more ports are available.
 * *******************************************************************/
static u16_t udp_new_port( NETIF_T* net_adapter)
{
#ifndef UDP_LOCAL_PORT_RANGE_START
#define UDP_LOCAL_PORT_RANGE_START 0x7100
#define UDP_LOCAL_PORT_RANGE_END   0x7fff
#endif
  int i;
  u16_t port = UDP_LOCAL_PORT_RANGE_START;
  UDP_T* udp_c_list = net_adapter->udp_c_list;

  T_ASSERT(("%s#%d More potential UDP connections than allowed port numbers.\r\n",__func__, __LINE__), MAX_UDP < UDP_LOCAL_PORT_RANGE_END-UDP_LOCAL_PORT_RANGE_START);

  for( i = 0; i < MAX_UDP; i++){
    if( udp_c_list[i].local_port == port){
      //The port is used so increment the port and check again from the beginning.
      port++;
      i = 0;
    }
  }

  return port;
}

/*!
 * Function name: udp_init_connection
 * \return ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param udp_c : [in/out] udp_c of interest.
 * \param dest_mac_addr : [in] Destination MAC address.
 * \brief cIPs limits the calculation. A communication between cIPS an a peer device
 * is an agreement. It is an agreement on the IP addresses and port numbers.
 * This agreement never changes so it is constant. udp_init_connection()
 * initializes the fields that are staying constant for the life of the connection
 * *******************************************************************/
static void udp_init_connection (UDP_T* const udp_c, const u8_t* const dest_mac_addr)
{
  UDP_HEADER_T *udphdr;

  if( !udp_c->frame_initialized ) {
    //Init the "segment of control" with everything that is known and constant: i.e. 
    // IP_HEADER_T:version_head_length,  IP_HEADER_T:type_of_service, IP_HEADER_T:id,
    // IP_HEADER_T:fragment_offset_field, IP_HEADER_T:time_to_live, IP_HEADER_T:protocol, IP_HEADER_T:source_addr, IP_HEADER_T:dest_addr,
    // UDP_HEADER_T:source_port, UDP_HEADER_T:dest_port. That 10 concatenated longs.

    u32_t dest_ip_or_gateway = arp_get_route(udp_c->remote_ip, udp_c->netif->gateway_addr,udp_c->netif->netmask);

    //Fill in ethernet part
    (void)eth_build_frame( dest_mac_addr, udp_c->netif->mac_address, dest_ip_or_gateway, udp_c->local_ip, udp_c->frame, ETH_ETHERNET);

    //Fill in IP part
    (void)ip_set_constant_fields( udp_c->remote_ip, udp_c->local_ip, udp_c->frame + sizeof(ETHER_HEADER_T), IP_UDP);

    //Fill in UDP part
    udphdr = (UDP_HEADER_T *)(udp_c->frame + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));
    udphdr->source_port = htons(udp_c->local_port);
    udphdr->dest_port = htons(udp_c->remote_port);

    udp_c->frame_initialized = TRUE;
  }

  return;
}
