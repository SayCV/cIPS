#ident "@(#) $Id: arp.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace arp
* \file arp.c
* \brief ARP layer
***************************************************/

/* libc Includes */
#include <string.h>

/* Stack Includes */
#include "basic_c_types.h"
#include "debug.h"
#include "nw_protocols.h"
#include "err.h"
#include "netif.h"
#include "arp.h"

/* External Declarations */

/* Internal Declarations */

#define ARP_UNUSED ((u32_t)~0)
static const u8_t BROADCAST_ADDR[MAC_ADDRESS_LENGTH] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const u8_t BLANK_ADDR[MAC_ADDRESS_LENGTH] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u32_t IP_BROADCAST = 0xFFFFFFFF;

static u32_t arp_build_frame(const u8_t* const dest_mac_addr, const u8_t* const source_mac_addr, const u32_t dest_ip_addr, const u32_t source_ip_addr, const u16_t arp_service, u8_t* const output_frame);
static void eth_memcpy(u8_t* output, const u8_t* input);

/*!
 * Function name: arp_init_cache
 * \param cache : [out] cache of interest.
 * \return nothing
 * \brief Reset the cache.
 * *******************************************************************/
void arp_init_cache(ARP_CACHE_T* const cache)
{
  u32_t i;

  //The ARP table is empty at start-up.
  for( i = 0; i < ARP_TABLE_SIZE; i++ )
  {
    cache->table[i].state = ARP_UNUSED;
    cache->table[i].ip_addr = ARP_UNUSED;
  }
  cache->older_index = 0;

  return ;
}

/*!
 * Function name: arp_parse
 * \return ERR_OK or ERR_DEVICE_DRIVER
 * \param eth_frame : [in] Ethernet frame.
 * \param net_adapter : [in] Adpater providing the frame.
 * \brief Parse the IP frame.
 * *******************************************************************/
err_t arp_parse(u8_t* eth_frame, NETIF_T *net_adapter)
{
  err_t err = ERR_OK;
  ARP_HEADER_T* arp_header = (ARP_HEADER_T*)((u8_t*)eth_frame + sizeof(ETHER_HEADER_T));
  if( ntohl(arp_header->target_ip_addr) == net_adapter->ip_addr) { //Update ARP table only if the peer device is the target.
    if(arp_header->operation == htons(ARP_REQUEST)) { //The peer device sends an arp request to cIPS. cIPS replies.
      u32_t framelen = eth_build_frame(arp_header->sender_hw_addr, net_adapter->mac_address, ntohl(arp_header->sender_ip_addr), net_adapter->ip_addr, net_adapter->control_buffer, ETH_ARP_REPLY);
      err = netif_send(net_adapter, net_adapter->control_buffer, framelen);
    } else if (arp_header->operation == htons(ARP_RESPONSE)) { //cIPS sent an arp request to a peer device. The peer device replied and here is the reply processing.
        err = arp_update_cache(&(net_adapter->arp_cache), ntohl(arp_header->sender_ip_addr), arp_header->sender_hw_addr);
    }
  }

  return err;
}

/*!
 * Function name: arp_update_cache
 * \return ERR_OK.
 * \param cache : [out] cache of interest.
 * \param ip_addr : [in] IP address of the adapter looked for.
 * \param source_mac_addr : [in] MAC address associated to the IP address.
 * \brief <br>
 * 1. Update the MAC address if the IP address already exists in the list.
 * 2. Otherwise, add the couple (IP addr, MAC addr) in the list
 * *******************************************************************/
err_t arp_update_cache(ARP_CACHE_T* const cache, const u32_t ip_addr, const u8_t* const source_mac_addr)
{
  u32_t i;
  err_t err = ERR_OK;
  bool_t found = FALSE;
  T_DEBUGF(ETHARP_DEBUG,("%s\r\n",__func__));

  T_DEBUGF(ETHARP_DEBUG,("ip_addr = %ld.%ld.%ld.%ld (0x%lx) ", (ip_addr >> 24) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 8) & 0xff, ip_addr & 0xff, ip_addr));
  T_DEBUGF(ETHARP_DEBUG,("mac %x.%x.%x.%x.%x.%x ",source_mac_addr[0],source_mac_addr[1],source_mac_addr[2],source_mac_addr[3],source_mac_addr[4],source_mac_addr[5]));
  T_DEBUGF(ETHARP_DEBUG,("%s \r\n",__func__));

  //Filter: check that the MAC addr is not a boadcast addr.
  i=0;
  while( (i < MAC_ADDRESS_LENGTH) && (source_mac_addr[i] == BROADCAST_ADDR[i]))
  {
    i++;
  }

  if( i != MAC_ADDRESS_LENGTH ) //!<If not a broadcast address.
  {
    //Scan the list of adapter to update the MAC address.
    for( i = 0; i < ARP_TABLE_SIZE; i++ )
    {
      if( cache->table[i].ip_addr == ip_addr )
      {
        (void)eth_memcpy(cache->table[i].ether_addr,source_mac_addr);
        found = TRUE;
        i = ARP_TABLE_SIZE; //exit loop
      }
    }

    if( !found )//If the IP address is not in the list then insert it.
    {
      for( i = 0; i < ARP_TABLE_SIZE; i++ )
      {
        if( cache->table[i].state == ARP_UNUSED )
        {
          (void)eth_memcpy(cache->table[i].ether_addr,source_mac_addr);
          cache->table[i].state = i;
          cache->table[i].ip_addr = ip_addr;
          found = TRUE;
          i = ARP_TABLE_SIZE; //exit loop
        }
      }
      if( !found ) //The table is full, an entry has to be replaced.
      { //Replace the oldest entry
        i = cache->older_index;
        (void)eth_memcpy(cache->table[i].ether_addr,source_mac_addr);
        cache->table[i].state = i;
        cache->table[i].ip_addr = ip_addr;
        (cache->older_index)++;
        if( cache->older_index == ARP_TABLE_SIZE){ //The table is a round buffer. Go back to the beginning when the index reaches the end.
          cache->older_index = 0;
        }
      }
    }
  }

  arp_print_cache(cache);
  return err;
}

/*!
 * Function name: arp_get_route
 * \return IP address of the gateway or the destination directly .
 * \param dest_ip_addr : [in] IP destination address.
 * \param gateway_addr : [in] Adapter gateway address. (gateway of 0 means "no gateway")
 * \param ip_mask : [in] IP adapter mask. 
 * \brief Decide whether the destination is out of the network.
 * In this case, the ethernet frame has to go through the gateway.
 * *******************************************************************/
u32_t arp_get_route(const u32_t dest_ip_addr, const u32_t gateway_addr, const u32_t ip_mask)
{
  u32_t ip_addr_to_reach;
  ip_addr_to_reach =(!gateway_addr || ( (dest_ip_addr & ip_mask) == (gateway_addr & ip_mask) || ( dest_ip_addr == IP_BROADCAST)))?dest_ip_addr:gateway_addr;

  return ip_addr_to_reach;
}

/*!
 * Function name: arp_query_cache
 * \return ERR_OK if found, ERR_VAL othewise.
 * \param cache : [in/out]  cache of interest.
 * \param ip_addr : [in] IP address of the adapter looked for.
 * \param destination_mac_addr : [out] MAC address associated to the IP address. 
 * \param ip_mask : [in] IP mask. 
 * \brief Look for the MAC address associated to an IP address.
 * *******************************************************************/
err_t arp_query_cache(ARP_CACHE_T* const cache, const u32_t ip_addr, u8_t* const destination_mac_addr, const u32_t ip_mask)
{
  u32_t i;
  err_t err = ERR_OK;
  bool_t found = FALSE;
  T_DEBUGF(ETHARP_DEBUG,("ip_addr = %ld.%ld.%ld.%ld ", (ip_addr >> 24) & 0xff, (ip_addr >> 16) & 0xff, (ip_addr >> 8) & 0xff, ip_addr & 0xff));
  T_DEBUGF(ETHARP_DEBUG,("(0x%lx) ", ip_addr));
  T_DEBUGF(ETHARP_DEBUG,("%s \r\n",__func__));
  //Scan the list of adapter to update the MAC address.
  for( i = 0; i < ARP_TABLE_SIZE; i++ )
  {
    if( cache->table[i].ip_addr == ip_addr )
    {
      (void)eth_memcpy(destination_mac_addr, cache->table[i].ether_addr);
      found = TRUE;
      i = ARP_TABLE_SIZE;  //exit loop
    }
  }
  
  if( !found )
  {
    //!<If broadcast address then this is not a "not found" and return the broadcast MAC address.
    if( (ip_addr & (~ip_mask)) != (IP_BROADCAST & (~ip_mask)))
    {err = ERR_VAL;}
    memset(destination_mac_addr, 0xFF ,MAC_ADDRESS_LENGTH);
    arp_print_cache(cache);
  }

  return err;
}

/*!
 * Function name: arp_print_cache
 * \param cache : [in] cache of interest.
 * \return nothing
 * \brief Reset the cache.
 * *******************************************************************/
#ifdef ETHARP_DEBUG
void arp_print_cache(const ARP_CACHE_T* const cache)
{
  u32_t i;
  u32_t max_display = 5; //Note: Restrict the display to "max_display" in order to limit traces taken by this function.

  if( ARP_TABLE_SIZE < max_display) { max_display = ARP_TABLE_SIZE;}
  
  T_DEBUGF(ETHARP_DEBUG,("ARP cache of %d elements:\r\n",ARP_TABLE_SIZE));
  //Scan the list of adapter to update the MAC address.
  for( i = 0; i < max_display; i++ )
  {
    T_DEBUGF(ETHARP_DEBUG,("%ld: state= %ld, ",i,(u32_t)cache->table[i].state));
    T_DEBUGF(ETHARP_DEBUG,("ip= %ld.%ld.%ld.%ld ",(cache->table[i].ip_addr >> 24) & 0xff, (cache->table[i].ip_addr >> 16) & 0xff, (cache->table[i].ip_addr >> 8) & 0xff, cache->table[i].ip_addr & 0xff));
    T_DEBUGF(ETHARP_DEBUG,(" (0x%x), ",cache->table[i].ip_addr));
    T_DEBUGF(ETHARP_DEBUG,("mac= %x.%x.%x.%x.%x.%x\r\n", cache->table[i].ether_addr[0],cache->table[i].ether_addr[1],cache->table[i].ether_addr[2],cache->table[i].ether_addr[3],cache->table[i].ether_addr[4],cache->table[i].ether_addr[5]));
  }

  return ;
}
#endif //ETHARP_DEBUG

/*!
 * Function name: eth_build_frame
 * \return length of the ethernet frame.
 * \param dest_mac_addr : [in] Destination MAC address expected in the the ethernet frame (1st elt of the ethernet frame).
 * \param source_mac_addr : [in] Source MAC address expected in the the ethernet frame (2nd elt of the ethernet frame).
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param output_frame : [out] Ethernet Frame generated.
 * \param protocol : [in] protocol of interest.
 * \brief Build the ethernet frame.
 * *******************************************************************/
u32_t eth_build_frame(const u8_t* const dest_mac_addr, const u8_t* const source_mac_addr,
  const u32_t dest_ip_addr, const u32_t source_ip_addr,
  u8_t* const output_frame, u32_t const protocol)
{
  ETHER_HEADER_T* ether;
  u32_t length = 0;

  //Ethernet header
  ether = (ETHER_HEADER_T *)output_frame;
  (void)eth_memcpy(ether->destination_addr,dest_mac_addr);
  (void)eth_memcpy(ether->source_addr,source_mac_addr);

  switch(protocol)
  {
    case ETH_ETHERNET:
      ether->frame_type = htons(ETHERTYPE_IP);
    break;  
    case ETH_ARP_REQUEST: //cIPS sent an arp request to a peer. The peer replied and here is the reply case.
      ether->frame_type = htons(ETHERTYPE_ARP);
      (void)eth_memcpy(ether->destination_addr,BROADCAST_ADDR);
      length = arp_build_frame(BLANK_ADDR, source_mac_addr,dest_ip_addr,source_ip_addr,ARP_REQUEST,output_frame+ sizeof(ETHER_HEADER_T));
      break;
    case ETH_ARP_REPLY: //The peer sends an arp request to cIPS. cIPS replies in this case.
      ether->frame_type = htons(ETHERTYPE_ARP);
      length = arp_build_frame(dest_mac_addr, source_mac_addr,dest_ip_addr,source_ip_addr,ARP_RESPONSE,output_frame+ sizeof(ETHER_HEADER_T));
    break;
    default:
    break;
  };

  length+= sizeof(ETHER_HEADER_T);

  return length ;
}

/*!
 * Function name: arp_build_frame
 * \return length of the arp frame.
  * \param dest_mac_addr : [in] Destination MAC address expected in the the ethernet frame (1st elt of the ethernet frame).
 * \param source_mac_addr : [in] Source MAC address expected in the the ethernet frame (2nd elt of the ethernet frame).
 * \param dest_ip_addr : [in] Destination IP address expected in the the IP frame (10th elt of the IP frame).
 * \param source_ip_addr : [in] Source IP address expected in the the IP frame (9th elt of the IP frame).
 * \param arp_service : [in] ARP_REQUEST or ARP_RESPONSE
 * \param output_frame : [out] Ethernet Frame generated.
 * \brief Build the ARP frame.
 * *******************************************************************/
u32_t arp_build_frame(const u8_t* const dest_mac_addr, const u8_t* const source_mac_addr,
  const u32_t dest_ip_addr, const u32_t source_ip_addr, const u16_t arp_service,
  u8_t* const output_frame)
{
  ARP_HEADER_T* arp;

  //Arp header  
  arp = (ARP_HEADER_T*) output_frame;
  arp->hardware = htons(ETHERTYPE_ETHERNET);
  arp->protocol = htons(ETHERTYPE_IP);
  arp->hw_size  = (u8_t) MAC_ADDRESS_LENGTH;
  arp->protocol_size = (u8_t)IP_ADDRESS_LENGTH ;
  arp->operation = htons(arp_service);
  (void)eth_memcpy(arp->sender_hw_addr,source_mac_addr);
  arp->sender_ip_addr = htonl(source_ip_addr);
  (void)eth_memcpy(arp->target_hw_addr,dest_mac_addr);
  arp->target_ip_addr = htonl(dest_ip_addr);

  return sizeof(ARP_HEADER_T);
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
