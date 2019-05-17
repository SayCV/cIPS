#ident "@(#) $Id: arp.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
* \file arp.h
* \brief ARP layer
***************************************************/
#ifndef _ARP_H_
#define _ARP_H_

/* libc Includes */

/* Stack Includes */
#include "nw_protocols.h"
#include "arch.h"

/* External Declarations */
struct NETIF_S;

/* Internal Declarations */

//!Services implemented for ARP and Ethernet
typedef enum  {
 ETH_ETHERNET,
 ETH_ARP_REQUEST,
 ETH_ARP_REPLY
} ETH_STATE_E;

//!ARP_CACHE_ENTRY_T describes an entry of the ARP table
typedef struct ARP_CACHE_ENTRY_S 
{
  u32_t state; //ARP_UNUSED or an index (the index is only a way to set the state to !ARP_UNUSED)
  u32_t ip_addr;
  u8_t ether_addr[MAC_ADDRESS_LENGTH];
}ARP_CACHE_ENTRY_T;

//!ARP cache.
typedef struct ARP_CACHE_S 
{
  ARP_CACHE_ENTRY_T table[ARP_TABLE_SIZE]; //it is the ARP table
  u32_t older_index; //when the table is full, the new entry replaces the older entry. "older_index" is its position in the ARP table.
}ARP_CACHE_T;

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Function name: arp_init_cache
 * \param cache : [out] cache of interest.
 * \return nothing
 * \brief Reset the cache.
 * *******************************************************************/
  void arp_init_cache(ARP_CACHE_T* const cache);

/*!
 * Function name: arp_parse
 * \return ERR_OK or ERR_DEVICE_DRIVER
 * \param eth_frame : [in] Ethernet frame.
 * \param net_adapter : [in] Adpater providing the frame.
 * \brief Parse the IP frame.
 * *******************************************************************/
  err_t arp_parse(u8_t* eth_frame, struct NETIF_S *net_adapter);

/*!
 * Function name: arp_update_cache
 * \return ERR_OK if an address is updated or inserted. ARP_ERR_NOK if the address cannot be inserted.
 * \param cache : [out] cache of interest.
 * \param ip_addr : [in] IP address of the adapter looked for.
 * \param source_mac_addr : [in] MAC address associated to the IP address.
 * \brief <br>
 * 1. Update the MAC address if the IP address already exists in the list.
 * 2. Otherwise, add the couple (IP addr, MAC addr) in the list
 * *******************************************************************/
  err_t arp_update_cache(ARP_CACHE_T* const cache, const u32_t ip_addr, const u8_t* const source_mac_addr);

/*!
 * Function name: arp_get_route
 * \return IP address of the gateway or the destination directly .
 * \param dest_ip_addr : [in] IP destination address.
 * \param gateway_addr : [in] Adapter gateway address.
 * \param ip_mask : [in] IP adapter mask. 
 * \brief Decide whether the destination is out of the network.
 * In this case, the ethernet frame has to go through the gateway.
 * *******************************************************************/
  u32_t arp_get_route(const u32_t dest_ip_addr, const u32_t gateway_addr,const u32_t ip_mask);

/*!
 * Function name: arp_query_cache
 * \return ERR_OK if found, ERR_VAL othewise.
 * \param cache : [in/out] cache of interest.
 * \param ip_addr : [in] IP address of the adapter looked for.
 * \param destination_mac_addr : [in] MAC address associated to the IP address. 
 * \param ip_mask : [in] IP mask. 
 * \brief Look for the MAC address associated to an IP address.
 * *******************************************************************/
  err_t arp_query_cache(ARP_CACHE_T* const cache, const u32_t ip_addr, u8_t* const destination_mac_addr, const u32_t ip_mask);

/*!
 * Function name: arp_print_cache
 * \param cache : [in] cache of interest.
 * \return nothing
 * \brief Reset the cache.
 * *******************************************************************/
#ifdef ETHARP_DEBUG
  void arp_print_cache(const ARP_CACHE_T* const cache);
#else //ETHARP_DEBUG
  #define arp_print_cache(cache)
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
 u32_t eth_build_frame(const u8_t* const dest_mac_addr, const u8_t* const source_mac_addr,const u32_t dest_ip_addr, const u32_t source_ip_addr,u8_t* const output_frame, u32_t const protocol);

#ifdef __cplusplus
}
#endif

#endif //_ARP_H_
