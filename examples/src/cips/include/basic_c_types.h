#ident "@(#) $Id: basic_c_types.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
#ifndef _BASIC_C_TYPES_H_
#define _BASIC_C_TYPES_H_


#ifndef NULL
  #define NULL 0
#endif

#ifdef __BIG_ENDIAN__
#ifndef htons
  #define htons(x) (x)
#endif

#ifndef ntohs
#define ntohs(x) (x)
#endif

#ifndef htonl
#define htonl(x) (x)
#endif

#ifndef ntohl
#define ntohl(x) (x)
#endif

#else 
#ifdef __LITTLE_ENDIAN__

#ifndef htons
#define htons(x) (unsigned short)(((x) << 8) | ((x) >> 8))
#endif

#ifndef ntohs
#define ntohs(x) htons(x)
#endif

#ifndef htonl
#define htonl(x) (unsigned long)(((x) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | ((x) >> 24))
#endif

#ifndef ntohl
#define ntohl(x) htonl(x)
#endif
#else
#error "Either -D__LITTLE_ENDIAN__ or -D__BIG_ENDIAN__ must be set in the compiler instructions."
#endif
#endif

/*!Redefinition of unsigned char */
#ifndef u8_t
  #define u8_t   unsigned char /*!<typedef  unsigned char  u8_t; */
#endif

/*!Redefinition of signed char */
#ifndef s8_t
  //Note: it would be better to define as "signed char" but mb-gcc 4.1.1 complains about that
  #define s8_t   char /*!<typedef  signed char    s8_t; */  
#endif

/*!Redefinition of unsigned short */
#ifndef u16_t
  #define u16_t  unsigned short int /*!<typedef  unsigned short u16_t; */
#endif

/*!Redefinition of signed short */
#ifndef s16_t
  #define s16_t  signed short int /*!<typedef  signed short   s16_t; */
#endif

/*!Redefinition of unsigned long */
#ifndef u32_t
  #define u32_t  unsigned long int /*!<typedef  unsigned long  u32_t; */
#endif

/*!Redefinition of signed long */
#ifndef s32_t
	#define s32_t  signed long int /*!<typedef  signed long    s32_t; */
#endif

/*!Redefinition of float */
#ifndef f32_t
  #define f32_t  float /*!<typedef  f32_t    float; */
#endif

/*!Redefinition of double */
#ifndef d32_t
  #define d32_t  signed long int /*!<typedef  double    d32_t; */
#endif

/*!Boolean representation */
#ifndef bool_t
  #define bool_t  u32_t
#endif

#ifndef TRUE
  #define TRUE 1
#endif

#ifndef FALSE
  #define FALSE 0
#endif

/*!Definition of err_t */
#ifndef err_t
  #define err_t  u32_t /*!<typedef u32_t err_t;*/
#endif

#endif /* _BASIC_C_TYPES_H_ */
