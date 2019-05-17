#ident "@(#) $Id: project_trace_level.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
/* Copyright (c) 2010, Jean-Marc David
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
* \namespace dt
* \file project_trace_level.h
* \brief  Module Description: 
* Define a hierarchie of debug messages for this project.
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


#ifndef __PROJECT_TRACE_LEVEL_H__
#define __PROJECT_TRACE_LEVEL_H__

/* libc Includes */

/* External Declarations */

/* Internal Declarations */

 
/******************** Application part *********************************/
/* Debug config */
#ifndef PROJECT_DEBUG
 #define PROJECT_DEBUG /*!< enable debug for the whole project*/
#endif 

#ifndef TRC_MIN_LEVEL /*!< minimum level of debug */
 #define TRC_MIN_LEVEL  TRC_LEVEL_INFO
// #define TRC_MIN_LEVEL  TRC_LEVEL_MISC
//  #define TRC_MIN_LEVEL  TRC_LEVEL_WARNING
#endif


/*! for network_adapter.c */
#ifndef NA_DEBUG
 #define NA_DEBUG  TRC_ON
#endif

/*! for connection_manager.c */
#ifndef WSCM_DEBUG
 #define WSCM_DEBUG  TRC_ON
#endif


/*! for web_server.c */
#ifndef WS_DEBUG
 #define WS_DEBUG  TRC_ON
#endif

/*! for web_connection.c */
#ifndef WEB_CON_DEBUG
  #define WEB_CON_DEBUG  TRC_ON
#endif

#endif /* __PROJECT_TRACE_LEVEL_H__ */






