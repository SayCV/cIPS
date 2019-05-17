#ident "@(#) $Id: application.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace wscm
* \file connection_manager.h
* \brief This module integrates the cIPS stack and a webserver <br>
* There are several possible configurations: <br>
* - No OS (Standalone) + no timer. <br>
* - Xilkernel (_XMK_) + no timer. <br>
* - Xilkernel (_XMK_) + timer. <br>
* I strongly recommend the latest configuration in order to handle
* TCP retransmissions and timeouts more accurately.
***************************************************/
#ifndef _WS_CONTROL_MANAGER_H_
#define _WS_CONTROL_MANAGER_H__

/* Includes */

/* External Declarations */

/* Internal Declarations */


#ifdef __cplusplus
extern "C" {
#endif
  void* wscm_start(void* arg);


#ifdef __cplusplus
}
#endif

#endif //_WS_CONTROL_MANAGER_H__
