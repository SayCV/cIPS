#ident "@(#) $Id: debug.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
#ifndef __T_DEBUG_H__
#define __T_DEBUG_H__

#include "arch.h"

#define DBG_ON  TRUE //!< Debug ON
#define DBG_OFF FALSE //!< Debug OFF

#ifdef T_DEBUG
#  define T_DEBUGF(debug,x) do { if ((debug) & DBG_ON) { T_PLATFORM_DIAG(x); } } while(0)
#  define T_ERROR(x)   do { T_PLATFORM_DIAG(x); } while(0)  
#  define T_ASSERT(x,y) do { if(!(y)) {T_PLATFORM_DIAG(x); T_PLATFORM_ASSERT(y);}} while(0)
#else // T_DEBUG
#  define T_DEBUGF(debug,x) 
#  define T_ERROR(x)  
#  define T_ASSERT(x,y) 
#endif // T_DEBUG


#endif // __T_DEBUG_H__

