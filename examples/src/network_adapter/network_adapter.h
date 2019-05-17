#ident "@(#) $Id: network_adapter.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \file network_adapter.h
* \brief  Module Description: 
* Sets a network adapter by setting the MAC, IP addresses and its interrupts.<br>
* It supports several adapters at the same time.<br>
***************************************************/
#ifndef _NETWORK_ADAPTER_H_
#define _NETWORK_ADAPTER_H_

/* libc Includes */

/* Xilinx Includes */

/* External Declarations */

/* Internal Declarations */

/*error values to be returned by functions*/
#define NA_OFFSET 0x4E413030 //!<ascii code of NA00

/*!Possible errors for this module */
typedef enum  {
 NA_ERR_OK =   0,  /*!< No error, everything OK. */
 NA_ERR_NOK = 0x01 + NA_OFFSET,  /*!< General error. */
 NA_ERR_MEM = 0x02 + NA_OFFSET,  /*!< Out of memory error. MAX_NET_ADAPTER adapters allocated*/
 NA_DEVICE_NOT_FOUND =  0x03 + NA_OFFSET, /*!< XST_DEVICE_NOT_FOUND from Xemaclite. */
 NA_REG_HANDLER =  0x04 + NA_OFFSET, /*!< register_int_handler() failed. */
 NA_RECV_CALLBACK =  0x05 + NA_OFFSET /*!< XEmacLite_SetRecvHandler() does set the received callback. */
} NA_ERROR_E;

#ifdef __cplusplus
extern "C" {
#endif

  void na_init(void);

#ifdef __XMK__
  NETIF_T* na_new(const s8_t* macaddr, const s8_t* ip_address,
    const s8_t* subnet,const s8_t* gateway_addr,
    const u16_t emac_deviceId, const int emac_interruptId, const bool_t optimized, const s8_t* const name, err_t* const err);

  u32_t na_delete(const u16_t emac_deviceId, const int emac_interruptId);
#else //__XMK__
  NETIF_T* na_new(const s8_t* macaddr, const s8_t* ip_address,
    const s8_t* subnet,const s8_t* gateway_addr,
    const u16_t emac_deviceId, const int emac_interruptId,
    const u32_t proc_interrupt_adress,
    const bool_t optimized, const s8_t* const name, err_t* const err);

  u32_t na_delete(const u16_t emac_deviceId, const u32_t proc_interrupt_adress , const u32_t proc_emac_mask_interrupt);
#endif //__XMK__

  void na_set_send_handler(const u16_t emac_deviceId, void (*handler)(void *CallBackRef));

#ifdef __cplusplus
}
#endif

#endif //_NETWORK_ADAPTER_H_

