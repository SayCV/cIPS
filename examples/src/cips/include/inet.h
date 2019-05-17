#ident "@(#) $Id: inet.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
#ifndef _INET_H_
#define _INET_H_

#include "arch.h"
#include "basic_c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Function name: inet_addr
 * \return ERR_OK if the "decimal_dot_address" is valid, ERR_VAL otherwise
 * \param decimal_dot_address : [in] IP address in a dotted decimal format.
 * \param long_address : [out] the IP address in a long format in the
 * endianness of the system.
 * \brief Transform a dotted decimal address (ex :10.2.222.222) to a long
 * *******************************************************************/
err_t inet_addr(const s8_t* const decimal_dot_address, u32_t* const long_address);

/*!
 * Function name: inet_ntoa
 * \return ERR_VAL if decimal_dot_address[] does not have memory allocated, ERR_OK otherwise,
 * \param decimal_dot_address : [out] IP address in a dotted decimal format.
 * \param long_address : [in] the IP address in a long format in the 
 * endianness of the system.
 * \brief Transform a long (ex: 0x0A02DEDEF) to a dotted 
 * decimal address (ex :10.2.222.223)
 * *******************************************************************/
err_t inet_ntoa(s8_t* const decimal_dot_address, const u32_t long_address);

/*!
 * Function name: inet_mac_address_from_decimal
 * \return ERR_OK if the "decimal_hyphen_mac_address" is valid, ERR_VAL otherwise
 * \param decimal_hyphen_mac_address : [in] mac address in a hyphened decimal format.
 * \param mac_address : [out] mac address in 6 bytes array.
 * \brief Transform a hyphened mac address (ex :211-22-33-44-55-66) to a 6 bytes array 
 * *******************************************************************/
err_t inet_mac_address_from_decimal(const s8_t* const decimal_hyphen_mac_address,  u8_t* const mac_address);

/*!
 * Function name: inet_mac_address_from_hex
 * \return ERR_OK if the "hexadecimal_hyphen_mac_address" is valid, ERR_VAL otherwise
 * \param hexadecimal_hyphen_mac_address : [in] mac address in a hyphened hexadecimal format.
 * \param mac_address : [out] mac address in 6 bytes array.
 * \brief Transform a hyphened mac address (ex :A1-0B-CC-DD-EE-FF) to a 6 bytes array 
 * *******************************************************************/
err_t inet_mac_address_from_hex(const s8_t* const hexadecimal_hyphen_mac_address,  u8_t* const mac_address);

#ifdef __cplusplus
}
#endif

#endif /* _INET_H_ */

