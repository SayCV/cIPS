#ident "@(#) $Id: inet.c,v 1.2 2013/12/12 20:22:25 jdavid Exp $"
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

#include "inet.h"
#include "err.h"

/*!
 * Function name: inet_addr
 * \return ERR_OK if the "decimal_dot_address" is valid, ERR_VAL otherwise
 * \param decimal_dot_address : [in] IP address in a dotted decimal format.
 * \param long_address : [out] the IP address in a long format in the
 * endianness of the system.
 * \brief Transform a dotted decimal address (ex :10.2.222.222) to a long
 * *******************************************************************/
err_t inet_addr(const s8_t* const decimal_dot_address, u32_t* const long_address)
{
  const s32_t MAX_IP_DIGITS = 3; // ddd
  const s32_t MAX_IP_QUARTET = 4; // [xx,xx,xx,xx]
  const u32_t MAX_IP_DEC_CHAR = 16; // ddd.ddd.ddd.ddd
  const u8_t DOT = '.';
  const u8_t ASCII_ZERO = '0';
  const u8_t ASCII_NINE = '9';
  const u32_t DEC_BASE = 10;
  u32_t j,k,l;
  u32_t intermediate_address;
  err_t err = ERR_OK;
  u8_t* p = (u8_t*)decimal_dot_address;

  j = 0;
  k = 0;
  l = 0;
  intermediate_address = 0;
  while(( k < MAX_IP_QUARTET ) && (l < MAX_IP_DEC_CHAR) && (!err)){
    if((*p != DOT) && (*p != 0)){
      if ((ASCII_ZERO <= *p) && (*p <= ASCII_NINE)) {
        intermediate_address *= DEC_BASE;
        intermediate_address += (*p - ASCII_ZERO);
        if( j++ == MAX_IP_DIGITS ){ //If the format is wrong (i.e. there is no '.')
          err = ERR_VAL;
        }
      }else{
        err = ERR_VAL;
      }
    } else {
      j=0;
      *long_address = *long_address << 8;
      *long_address += intermediate_address;
      k++;
      intermediate_address = 0;
    }
    l++;
    p++;
  }

  if(k != MAX_IP_QUARTET){ //If the format is wrong, it did not get 4 "xx".
    err = ERR_VAL;
  }
  return err;
}


/*!
 * Function name: inet_ntoa
 * \return ERR_VAL if decimal_dot_address[] does not have memory allocated, ERR_OK otherwise,
 * \param decimal_dot_address : [out] IP address in a dotted decimal format.
 * \param long_address : [in] the IP address in a long format in the 
 * endianness of the system.
 * \brief Transform a long (ex: 0x0A02DEDF or 167960287 in decimal)
 * to a dotted decimal address (ex :10.2.222.223)
 * *******************************************************************/
err_t inet_ntoa(s8_t* const decimal_dot_address, const u32_t long_address)
{
  const u32_t MAX_IP_DIGITS = 3; // xxx
  const u32_t MAX_IP_QUARTET = 4; // xxx.xxx.xxx.xxx
  const u8_t USELESS_ZERO = 'X'; //Leading zero identifier
  const u8_t DOT = '.';
  const u8_t ASCII_ZERO = '0';
  const u32_t DEC_BASE = 10;
  u32_t j,d,q;
  u8_t address[MAX_IP_QUARTET];
  u8_t address_string[MAX_IP_QUARTET][MAX_IP_DIGITS];
  u32_t long_addr;
  err_t err = ERR_OK;

  //Ex: the decimal input 167960287 or 0x0A02DEDF is transformed into 
  //"0x0A.0x02.0xDE.0xDF" then into "10.2.222.223"
  long_addr = long_address;
  //Split 0x0A02DEDF to address[]={0A,02,DE,DF};
  for(q=0; q<MAX_IP_QUARTET; q++){
    address[MAX_IP_QUARTET-1 - q] = long_addr & 0xFF;
    long_addr = long_addr >> 8;
  }

  for(q=0; q<MAX_IP_QUARTET; q++){
    for(d=0; d<MAX_IP_DIGITS; d++){
      address_string[MAX_IP_QUARTET-1-q][MAX_IP_DIGITS-1-d] = address[MAX_IP_QUARTET-1 - q] - ((address[MAX_IP_QUARTET-1 - q]/DEC_BASE)*DEC_BASE);   
      address[MAX_IP_QUARTET-1 - q] /= DEC_BASE;
    }
  }
  //Remove the leading 0s. Ex: 010.002.222.223 becomes X10.X02.222.223
  for(q=0; q<MAX_IP_QUARTET; q++){
    if(address_string[MAX_IP_QUARTET-1-q][0] == 0){
      address_string[MAX_IP_QUARTET-1-q][0] = USELESS_ZERO;
    }
  }
  //Remove the 0s of the middle. Ex: X10.X02.222.223 becomes X10.XX2.222.223
  for(q=0; q<MAX_IP_QUARTET; q++){
    if((address_string[MAX_IP_QUARTET-1-q][0] == USELESS_ZERO) && (address_string[MAX_IP_QUARTET-1-q][1] == 0)){
      address_string[MAX_IP_QUARTET-1-q][1] = USELESS_ZERO;
    }
  }

  if(decimal_dot_address){
    //Pack the string, i.e. remove the leading 0s. Ex: 010.002.222.223 becomes 10.2.222.223
    j = 0;
    for(q=0; q<MAX_IP_QUARTET; q++){
      for(d=0; d<MAX_IP_DIGITS; d++){
        if(address_string[q][d] !=USELESS_ZERO){
          decimal_dot_address[j] = address_string[q][d] + ASCII_ZERO;
          j++;
        }
      }
      decimal_dot_address[j] = DOT;
      j++;
    }
    decimal_dot_address[j-1] = 0; //End of string
  } else {
    err = ERR_VAL;
  }
  return err;
}

/*!
 * Function name: inet_mac_address_from_decimal
 * \return ERR_OK if the "decimal_hyphen_mac_address" is valid, ERR_VAL otherwise
 * \param decimal_hyphen_mac_address : [in] mac address in a hyphened decimal format.
 * \param mac_address : [out] mac address in 6 bytes array.
 * \brief Transform a hyphened mac address (ex :211-22-33-44-55-66) to a 6 bytes array 
 * *******************************************************************/
err_t inet_mac_address_from_decimal(const s8_t* const decimal_hyphen_mac_address,  u8_t* const mac_address)
{
  const u32_t MAX_MAC_DIGITS = 3; // xxx
  const u32_t MAX_MAC_ELEMENTS = 6; // [xx,xx,xx,xx,xx,xx]
  const u32_t MAX_MAC_DEC_ELEMENT = 24; // ddd-ddd-ddd-ddd-ddd-ddd0
  u32_t j,k,l;
  u32_t intermediate_address;
  err_t err = ERR_OK;
  u8_t* p = (u8_t*)decimal_hyphen_mac_address;

  j = 0;
  k = 0;
  l = 0;
  intermediate_address = 0;
  while(( k < MAX_MAC_ELEMENTS ) && (l < MAX_MAC_DEC_ELEMENT) && (!err)){
    if((*p != '-') && (*p != 0)){
      intermediate_address *= 10;
      intermediate_address += (*p - '0');
      if( j++ == MAX_MAC_DIGITS ){ //If the format is wrong (i.e. there is no '.')
        err = ERR_VAL;
      }
    } else {
      if( intermediate_address > 0xFF){
        err = ERR_VAL;
      }
      j=0;
      mac_address[k] = intermediate_address;
      k++;
      intermediate_address = 0;
    }
    p++;
    l++;
  }
  if(k != MAX_MAC_ELEMENTS){ //If the format is wrong, it did not get 6 "xx".
    err = ERR_VAL;
  }
  return err;
}

/*!
 * Function name: inet_mac_address_from_hex
 * \return ERR_OK if the "hexadecimal_hyphen_mac_address" is valid, ERR_VAL otherwise
 * \param hexadecimal_hyphen_mac_address : [in] mac address in a hyphened hexadecimal format.
 * \param mac_address : [out] mac address in 6 bytes array.
 * \brief Transform a hyphened mac address (ex :A1-0B-CC-DD-EE-FF) to a 6 bytes array 
 * *******************************************************************/
err_t inet_mac_address_from_hex(const s8_t* const hexadecimal_hyphen_mac_address,  u8_t* const mac_address)
{
  const u32_t MAX_MAC_DIGITS = 2; // xx
  const u32_t MAX_MAC_ELEMENTS = 6; // [xx,xx,xx,xx,xx,xx]
  const u32_t MAX_MAC_HEXA_ELEMENT = 18; // xx-xx-xx-xx-xx-xx\0
  u32_t j,k,l;
  u32_t intermediate_address;
  err_t err = ERR_OK;
  u8_t* p = (u8_t*)hexadecimal_hyphen_mac_address;

  j = 0;
  k = 0;
  l = 0;
  intermediate_address = 0;
  while(( k < MAX_MAC_ELEMENTS ) && (l < MAX_MAC_HEXA_ELEMENT) && (!err)){
    if(((*p != '-')&&(*p != ':')) && (*p != 0) ){
      intermediate_address *= 16;
      if ((*p >= 'A') && (*p <='F')) {
        intermediate_address += *p - 'A' + 0xA;
      } else if ((*p >= 'a') && (*p <='f')) {
        intermediate_address += *p - 'a' + 0xA;
      } else if ((*p >= '0') && (*p <='9')) {
        intermediate_address += *p - '0';
      } else {
        intermediate_address = 0;
        err = ERR_VAL;
      }

      if( j++ == MAX_MAC_DIGITS ){ //If the format is wrong (i.e. there is no '.')
        err = ERR_VAL;
      }
    } else {
      j=0;
      mac_address[k] = intermediate_address;
      k++;
      intermediate_address = 0;
    }
    l++;
    p++;
  }
  if(k != MAX_MAC_ELEMENTS){ //If the format is wrong, it did not get 6 "xx".
    err = ERR_VAL;
  }
  return err;
}

