#ident "@(#) $Id: basic_types.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace  bt
* \file basic_types.h
* \brief  Module Description: 
* This module redefines basic type definitions.
* \note 
***************************************************/
#ifndef _BASIC_TYPES_H_
#define _BASIC_TYPES_H_

/*!Definition for true*/
#ifndef TRUE
#define TRUE  1
#endif
/*!Definition for false */
#ifndef FALSE
#define FALSE  (!TRUE)
#endif
/*!Defintion of null */
#ifndef NULL
#define NULL  0
#endif
/*!Definition for set*/
#ifndef SET
#define SET  TRUE
#endif
/*!Definition for clear */
#ifndef CLEAR
#define CLEAR  FALSE
#endif

#ifndef __ARCH_CC_H__
/*!Redefinition of the basic types */

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
#endif

/*!Redefinition of float */
#ifndef f32_t
  #define f32_t  float
#endif

/*!Redefinition of double */
#ifndef d32_t
  #define d32_t  double
#endif

/*!Definition of boolean */
#ifndef bool_t
  #define bool_t  u32_t
#endif

#endif  /*_BASIC_TYPES_H_ */

