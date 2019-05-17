#ident "@(#) $Id: err.h,v 1.2 2013/12/04 20:50:55 jdavid Exp $"
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
* \namespace err
* \file err.h
* \brief  Module Description: Error strategy for the stack.
***************************************************/

#ifndef __ERR_H__
#define __ERR_H__

#define ERR_OFFSET 0xF0
//!< Error definition.
typedef enum STACK_ERROR_E {
 ERR_OK   =0,      //No error, everything OK.
 ERR_APP   =1,      // Application error.
 ERR_MIN = ERR_OFFSET, //MIN_INDEX.
 ERR_BUF = ERR_OFFSET +2,      // Buffer error.
 ERR_RST = ERR_OFFSET +3,      // Connection reset.
 ERR_VAL = ERR_OFFSET +4,      // Illegal value.
 ERR_MAC_ADDR_UNKNOWN = ERR_OFFSET +5,   // MAC address unknown: need to send an ARP msg.
 ERR_CHECKSUM = ERR_OFFSET +6,   // The receiving checksum is wrong.
 ERR_STREAMING = ERR_OFFSET +7,   // The receiving stream is incomplete.
 ERR_PEER_WINDOW = ERR_OFFSET +8,   // The window of cIPS' peer device on the network equals 0.
 ERR_NET_ADAPTER_MEM = ERR_OFFSET +9,   // Max network adapters used reached. Increase MAX_NET_ADPATER.
 ERR_TCP_MEM = ERR_OFFSET +10,   // Max TCP controllers used reached. Increase MAX_TCP.
 ERR_SEG_MEM = ERR_OFFSET +11,   // Payload too large for the TCP controller. Increase TCP_SEG_NB.
 ERR_CUR_SEG_MEM = ERR_OFFSET +12,   // Payload too large for the TCP controller at the moment.
 ERR_UDP_MEM = ERR_OFFSET +13,   // Max UDP controllers used reached. Increase MAX_UDP.
 ERR_DEVICE_DRIVER = ERR_OFFSET +14,   // Device driver error.
 ERR_MAX = ERR_OFFSET +15 //MAX_INDEX.
} STACK_ERROR_T;

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Function name: decode_err
 * \return formated string with the error.
 * \brief descriptions: Report an error formated in a string.<br>
 * cIPS errors are between ERR_MIN and ERR_MAX, the application can
 * use any other value.
 * *******************************************************************/
  s8_t* decode_err( const err_t err );

#ifdef __cplusplus
}
#endif

#endif /* __ERR_H__ */
