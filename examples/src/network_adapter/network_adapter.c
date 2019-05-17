#ident "@(#) $Id: network_adapter.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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

* Author: Jean-Marc David
*/


/*!
* \namespace na
* \file network_adapter.c
* \brief  Module Description: 
* Link a device driver to the TCP-IP library.
***************************************************/

#include "xemaclite.h"
#ifdef __XMK__
#include "sys/intr.h" //for register_int_handler
#else
#include "xparameters.h"
#include "xintc.h"
#include "xtmrctr_l.h"
#endif //__XMK__
#include "cips.h"
#include "network_adapter.h"

/* External Declarations */

/* Internal Declarations */
#define MAC_ADDR_LENGTH 6 /*!< A mac address consist of 6 numbers*/

/*! Link between the device driver and the TCP-IP library */
typedef struct ADAPTER_LINK_S{
  NETIF_T* server_netif; //!< TCP-IP library interface.
  XEmacLite xemac; //!< Device driver network interface.
} ADAPTER_LINK_T;

static u32_t na_fifo_recv(void* pDriver_arg, u8_t* frame);
static u32_t na_fifo_send(void* pDriver_arg, u8_t* frame, u32_t frame_length);
static void na_stub_handler(void *CallBackRef);

ADAPTER_LINK_T g_adapter[MAX_NET_ADAPTER]; /*!<Array of network adapters.*/


/*!
 * Function name: na_init
 * \return: nothing
 * \brief description: Initialize the structure which will be the link 
 * between abstract network adapter and the device driver.
 * *******************************************************************/
void na_init(void)
{
  u32_t i;
  for ( i = 0; i < MAX_NET_ADAPTER; i++)
  {
    g_adapter[i].server_netif = NULL;
  }
  (void)netif_init();
  return;
}

#ifdef __XMK__
/*!
 * Function name: na_new
 * \return: a pointer to a logical network adapter (lib) or NULL in case of failure.
 * \param macaddr : [in] MAC address
 * \param ip_address : [in] ip address
 * \param subnet :[in] subnet
 * \param gateway_addr :[in] gateway addr
 * \param emac_deviceId : [in] index of network interface.
 * \param emac_interruptId : [in] interrupt ID related to the ethenet HW number "emac_deviceId"
 * \param optimized : [in] Flag (default is FALSE). If TRUE, UDP are process in the Adapter ISR. If FALSE, a filter is apply to avoid unwanted frames.
 * \param name : [in] Name given to the adapter. Restricted to 2 letters.
 * \param err : [out] Error code: NA_ERR_OK if OK, otherwise NA_ERR_MEM, NA_DEVICE_NOT_FOUND, NA_REG_HANDLER or NA_RECV_CALLBACK.
 * \brief description: set up the network interface (i.e. set up a network HW with a MAC
 * and IP address and set the network interrupt).
 * \note nothing
 * *******************************************************************/
NETIF_T* na_new(const s8_t* macaddr, const s8_t* ip_address,
  const s8_t* subnet,const s8_t* gateway_addr,
  const u16_t emac_deviceId, const int emac_interruptId, const bool_t optimized, const s8_t* const name, err_t* const err)
{
  u32_t ipaddr, netmask, gateway;
  u8_t mac_address[MAC_ADDR_LENGTH];
  NETIF_T* pnetif = NULL;

  *err = inet_mac_address_from_hex(macaddr,mac_address);

  // Read in and set the IP address, Subnet Mask, and Gateway
  if(!*err){
    *err = inet_addr(ip_address, &ipaddr);
  }
  if(!*err){
    *err = inet_addr(subnet, &netmask);
  }
  if(!*err){
    *err = inet_addr(gateway_addr, &gateway);
  }

  //Get Adapter base address.
  if(!*err){
    *err = XEmacLite_Initialize(&g_adapter[emac_deviceId].xemac, emac_deviceId);
  }
  if(!*err)
  {
    //Link netif to the device driver
    g_adapter[emac_deviceId].server_netif = netif_new( mac_address, ipaddr, netmask, gateway, name, optimized,
      na_fifo_recv, na_fifo_send, &g_adapter[emac_deviceId].xemac, err);
    if(!*err)
    {
      // Register the XEMAC interrupt handler with the controller and enable interrupts within XMK
      *err = register_int_handler(emac_interruptId, (XInterruptHandler)XEmacLite_InterruptHandler,
                       &g_adapter[emac_deviceId].xemac); //returns XST_SUCCESS or -1 if (TIMER_INTERRUPT_ID == SYSTMR_INTR_ID). SYSTMR_INTR_ID equals 0.
      if(!*err)
      {
        //Link the device driver to netif
        (void) XEmacLite_SetMacAddress(&g_adapter[emac_deviceId].xemac, mac_address);
        if( optimized == TRUE)
        {(void)XEmacLite_SetRecvHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) netif_ISR_optimized);}
        else
        {(void)XEmacLite_SetRecvHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) netif_ISR);}

        (void)XEmacLite_SetSendHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) na_stub_handler);
        (void)XEmacLite_FlushReceive(&g_adapter[emac_deviceId].xemac); //!<Empty any existing receive frames.
        *err = XEmacLite_EnableInterrupts(&g_adapter[emac_deviceId].xemac); //!Start the EMACLite controller
        if(!*err)
        { pnetif = g_adapter[emac_deviceId].server_netif;}
        else
        { *err = NA_RECV_CALLBACK;}

        (void) enable_interrupt(emac_interruptId);
      }
      else
      { *err = NA_REG_HANDLER;}
    }
    else
    {
      *err = NA_ERR_MEM; //!<MAX_NET_ADAPTER adapters allocated. Increase MAX_NET_ADAPTER.
    }
  }
  else
  {
    *err = NA_DEVICE_NOT_FOUND;
  }

  return pnetif;
}

/*!
 * Function name: na_delete
 * \return: NA_ERR_OK
 * \param emac_deviceId : [in] index of network interface.
 * \param emac_interruptId : [in] interrupt ID related to the ethenet HW number "emac_deviceId"
 * \brief description: delete the network interface.
 * \note nothing
 * *******************************************************************/
u32_t na_delete(const u16_t emac_deviceId, const int emac_interruptId)
{
  u32_t err = NA_ERR_OK;

  (void) disable_interrupt(emac_interruptId);
  (void) unregister_int_handler(emac_interruptId);

  (void) XEmacLite_DisableInterrupts(&g_adapter[emac_deviceId].xemac);
  (void) netif_delete(g_adapter[emac_deviceId].server_netif);

  return err;
}
#else //__XMK__

/*!
 * Function name: na_new
 * \return a pointer to a logical network adapter (lib) or NULL in case of failure.
 * \param macaddr : [in] MAC address
 * \param ip_address : [in] ip address
 * \param subnet :[in] subnet
 * \param gateway_addr :[in] gateway addr
 * \param emac_deviceId : [in] index of network interface.
 * \param emac_interruptId : [in] indentify the kind of interrupt or the caller and the action that
 * the processor should execute.
 * \param proc_interrupt_adress : [in] interrupt address or global interrupt address of the processor.
 * \param optimized : [in] Flag (default is FALSE). If TRUE, UDP are process in the Adapter ISR. If FALSE, a filter is apply to avoid unwanted frames.
 * \param name : [in] Name given to the adapter. Restricted to 2 letters.
 * \param err : [out] Error code: NA_ERR_OK if OK, otherwise NA_ERR_MEM, NA_DEVICE_NOT_FOUND, NA_REG_HANDLER or NA_RECV_CALLBACK.
 * \brief Set up the network interface (i.e. set up a network HW with a MAC
 * and IP address and set the network interrupt).
 * \note After calling this function, the interrupt controller (XIntc_MasterEnable) 
 * must be started and the EMAC interrupt enabled (XIntc_EnableIntr).
 * *******************************************************************/
NETIF_T* na_new(const s8_t* macaddr, const s8_t* ip_address,
  const s8_t* subnet,const s8_t* gateway_addr,
  const u16_t emac_deviceId, const int emac_interruptId,
  const u32_t proc_interrupt_adress ,
  const bool_t optimized, const s8_t* const name, err_t* const err)
{
  u32_t ipaddr, netmask, gateway;
  u8_t mac_address[MAC_ADDR_LENGTH];
  NETIF_T* pnetif = NULL;

  *err = inet_mac_address_from_hex(macaddr,mac_address);

  // Read in and set the IP address, Subnet Mask, and Gateway
  if(!*err){
    *err = inet_addr(ip_address, &ipaddr);
  }
  if(!*err){
    *err = inet_addr(subnet, &netmask);
  }
  if(!*err){
    *err = inet_addr(gateway_addr, &gateway);
  }

  //Get Adapter base address.
  if(!*err){
    *err = XEmacLite_Initialize(&g_adapter[emac_deviceId].xemac, emac_deviceId);
  }
  if(!*err)
  {
    //Link netif to the device driver
    g_adapter[emac_deviceId].server_netif = netif_new( mac_address, ipaddr, netmask, gateway, name, optimized,
      na_fifo_recv, na_fifo_send, &g_adapter[emac_deviceId].xemac, err );

    if(g_adapter[emac_deviceId].server_netif != NULL)
    {
      // Register the XEMAC interrupt handler with the processor
      // - The processor handles one interrupt at a time.
      // The interrupt refers to the global interrupt enable (GIE) bit.
      // - However, several devices generate interrupts.
      // - The processor identifies the device with the interrupt_ID.
      // - A interrupt leads to an action. The interrupt handler is the action.
      // - XIntc_RegisterHandler() links the interrupt_ID to the interrpt handler.
      // So, a device generates an interrupts, the processor interrupts, look at
      // the ID and executes the action associated to the ID.
      (void)XIntc_RegisterHandler(proc_interrupt_adress,emac_interruptId,
           (XInterruptHandler)XEmacLite_InterruptHandler,
           &g_adapter[emac_deviceId].xemac);

      //Link the device driver to netif
      (void) XEmacLite_SetMacAddress(&g_adapter[emac_deviceId].xemac, mac_address);
      if( optimized == TRUE)
      {(void)XEmacLite_SetRecvHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) netif_ISR_optimized);}
      else
      {(void)XEmacLite_SetRecvHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) netif_ISR);}

      (void)XEmacLite_SetSendHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) na_stub_handler);

      (void)XEmacLite_FlushReceive(&g_adapter[emac_deviceId].xemac); //!<Empty any existing receive frames.
      *err = XEmacLite_EnableInterrupts(&g_adapter[emac_deviceId].xemac); //!Start the EMACLite controller
      if(!*err)
      { pnetif = g_adapter[emac_deviceId].server_netif;}
      else
      { *err = NA_RECV_CALLBACK;}
    }
    else
    {
      *err = NA_ERR_MEM; //!<MAX_NET_ADAPTER adapters allocated. Increase MAX_NET_ADAPTER.
    }
  }
  else
  {
    *err = NA_DEVICE_NOT_FOUND;
  }

  return pnetif;
}

/*!
 * Function name: na_delete
 * \return: NA_ERR_OK
 * \param emac_deviceId : [in] index of network interface.
 * \param proc_interrupt_adress : [in] interrupt address of the processor.
 * \param proc_emac_mask_interrupt : [in] mask corresponding to the emac interrupt.
 * \brief description: delete the network interface.
 * \note nothing
 * *******************************************************************/
u32_t na_delete(const u16_t emac_deviceId, const u32_t proc_interrupt_adress , const u32_t proc_emac_mask_interrupt)
{
  u32_t err = NA_ERR_OK;

  (void)XIntc_DisableIntr(proc_interrupt_adress, proc_emac_mask_interrupt);

  (void) XEmacLite_DisableInterrupts(&g_adapter[emac_deviceId].xemac);
  (void) netif_delete(g_adapter[emac_deviceId].server_netif);

  return err;
}

#endif //__XMK__
/*!
 * Function name: na_set_send_handler
 * \return: nothing
 * \param emac_deviceId : [in] index of network interface.
 * \param handler : [in] function to be called after that the device driver sends a frame.
 * \brief description: Sets the handler to be called after completion 
 * of sending a ethernet frame by the device driver.
 * \note nothing
 * *******************************************************************/
void na_set_send_handler(const u16_t emac_deviceId, void (*handler)(void *CallBackRef))
{
  (void)XEmacLite_SetSendHandler(&g_adapter[emac_deviceId].xemac, g_adapter[emac_deviceId].server_netif, (XEmacLite_Handler) handler);
  return;
}

/*!
 * Function name: na_fifo_recv
 * \return: size of the ethernet frame read.
 * \param pDriver_arg : [in] device driver instance.
 * \param frame : [out] ethernet frame read.
 * \brief description: Encapsulate the emac device driver.
 * \note : this encapsulation is not needed with emaclite_1.01.b (so it's faster!)
 * *******************************************************************/
static u32_t na_fifo_recv(void* pDriver_arg, u8_t* frame)
{
  return (u32_t)XEmacLite_Recv((XEmacLite*) pDriver_arg, frame);
}

/*!
 * Function name: na_fifo_send
 * \return: status of the sending.
 * \param pDriver_arg : [in] device driver instance.
 * \param frame : [in] ethernet frame to be sent out.
 * \param frame_length : [in] ethernet frame length.
 * \brief description: Encapsulate the emac device driver.
 * \note : this encapsulation is not needed with emaclite_1.01.b (so it's faster!)
 * *******************************************************************/
static u32_t na_fifo_send(void* pDriver_arg, u8_t* frame, u32_t frame_length)
{
  return XEmacLite_Send((XEmacLite*) pDriver_arg, frame, frame_length);
}

/*!
 * Function name: na_stub_handler
 * \return: mothing.
 * \param CallBackRef : [in] anything.
 * \brief Send callback.
 * \note : this encapsulation is not needed with emaclite_1.01.b (so it's faster!)
 * *******************************************************************/
static void na_stub_handler(void *CallBackRef)
{
}

