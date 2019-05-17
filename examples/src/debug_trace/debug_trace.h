#ident "@(#) $Id: debug_trace.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace dt
* \file debug_trace.h
* \brief  Module Description: 
* Define a hierarchie of debug messages
*
* lower two bits indicate debug level
* - TRC_LEVEL_MISC : everything
* - TRC_LEVEL_INFO : progress information
* - TRC_LEVEL_WARNING : warning
* - TRC_LEVEL_ERROR : serious
* <p> Usage: <br>
* \#ifndef NA_DEBUG<br>
*  \#define NA  TRC_ON<br>
* \#endif<br>
*
* \#ifndef TRC_MIN_LEVEL<br>
*  \#define TRC_MIN_LEVEL      TRC_LEVEL_MISC<br>
* \#endif<br>
*
* DT_TRACE(NA_DEBUG, ("Webserver debug\n"));
* DT_TRACE(NA_DEBUG, ("Webserver timer: %u.\n", i));</p>
***************************************************/

#ifndef __DEBUG_TRACE_H__
#define __DEBUG_TRACE_H__

#include <stdio.h>
#include <assert.h>
#include "project_trace_level.h"

/******************** Debug macros ************************************/ 

/* DEBUG LEVEL */
#define TRC_LEVEL_MISC    0 //!<everything
#define TRC_LEVEL_INFO    1 //!<progress information
#define TRC_LEVEL_WARNING 2 //!<warning
#define TRC_LEVEL_ERROR   3 //!<error
#define TRC_MASK_LEVEL    TRC_LEVEL_ERROR  //!<filter level for the whole application

#define TRC_ON  0x80U //!<flag for DEBUGF to enable that debug message
#define TRC_OFF 0x00U //!<flag for DEBUGF to disable that debug message
#define TRC_HALT    0x08U //!<flag for DT_TRACE to halt after printing this debug message

#ifndef DT_PLATFORM_ASSERT
 #define DT_PLATFORM_ASSERT(x) assert(x)
#endif

#ifndef DT_PLATFORM_DIAG
 #define DT_PLATFORM_DIAG(x)  printf x
#endif

#ifdef PROJECT_DEBUG
 /*! DT_ASSERT exists up to TRC_LEVEL_WARNING (included)*/
 #define DT_ASSERT(debug,x) do { if (((debug) & TRC_ON) && ((TRC_LEVEL_WARNING) >= TRC_MIN_LEVEL))  {DT_PLATFORM_ASSERT(x); } }while(0)
/* print debug message only if debug message type is enabled...
 *  AND is of correct type AND is at least TRC_LEVEL
 */
 #define DT_TRACE(debug,x) do { if (((debug) & TRC_ON) && (((debug) & TRC_MASK_LEVEL) >= TRC_MIN_LEVEL))  {DT_PLATFORM_DIAG(x); if ((debug) & TRC_HALT) while(1); } }while(0)
 #define DT_WARNING(x) DT_TRACE(TRC_ON|TRC_LEVEL_WARNING,x)
 #define DT_ERROR(x)	 do {DT_PLATFORM_DIAG(x);}while(0)
#else /* PROJECT_DEBUG */
 #define DT_ASSERT(debug,x)
 #define DT_TRACE(debug,x)
 #define DT_WARNING(x)
 #define DT_ERROR(x)
#endif /* PROJECT_DEBUG */


#endif /* __DEBUG_TRACE_H__ */






