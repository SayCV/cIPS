#ident "@(#) $Id: err.c,v 1.2 2013/12/04 20:50:42 jdavid Exp $"
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
* \file err.c
* \brief Module Description: Error strategy for the stack.
***************************************************/

#include "basic_c_types.h"
#include "err.h"

static char *formatted_error[] = {
  "Ok.", //ERR_OK
  "Application error.", //ERR_APP
  "Buffer error.", //ERR_BUF
  "Connection reset.", //ERR_RST
  "Illegal value.", //ERR_VAL
  "Dest MAC address unknown.", //ERR_MAC_ADDR_UNKNOWN
  "Checksum error.", //ERR_CHECKSUM
  "The receiving stream is incomplete.", //ERR_STREAMING
  "Window of the peer device cannot accept more data.", //ERR_PEER_WINDOW
  "Max network adapters used reached. Increase MAX_NET_ADPATER", //ERR_NET_ADAPTER_MEM
  "Max TCP controllers used reached. Increase MAX_TCP", //ERR_TCP_MEM
  "Not enough segments available. Payload may be too large for the TCP controller. Increase MAX_TCP_SEG", //ERR_SEG_MEM
  "Payload too large for the TCP controller at the moment.", //ERR_CUR_SEG_MEM
  "Max UDP controllers used reached. Increase MAX_UDP", //ERR_UDP_MEM
  "Device driver error. " //ERR_DEVICE_DRIVER
};

/*!
 * Function name: decode_err
 * \return formated string with the error.
 * \brief Report an error formated in a string.<br>
 * cIPS errors are between ERR_MIN and ERR_MAX, the application can
 * use any other value.
 * *******************************************************************/
s8_t* decode_err( const err_t err )
{
  u32_t index;
  if( (err > ERR_MIN) && (err < ERR_MAX))
  { index = err - ERR_OFFSET;}
  else if( err == ERR_OK)
  { index = 0;}
  else { index = ERR_APP;}

  return formatted_error[index];
}
