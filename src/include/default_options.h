#ident "@(#) $Id: default_options.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
* \namespace opt
* \file default_options.h
* \brief  Default parameters for the stack. The parameters can be overriden on the application
* basis by "application_options.h".
***************************************************/

#ifndef __DEFAULT_OPTIONS_H__
#define __DEFAULT_OPTIONS_H__

/* RECV_BUF_SIZE: Max nb of incoming ethernet frames stored*/
#ifndef RECV_BUF_SIZE
#define RECV_BUF_SIZE                   10
#endif

/* NETWORK_MTU: Size in bytes of an ethernet frame for the device driver. */
#ifndef NETWORK_MTU
#define NETWORK_MTU 1518 //!<Maximum Transmission Unit (MTU) refers to the size (in bytes) of the largest packet that a given layer of a communications protocol can pass onwards
#endif

/* MAX_NET_ADAPTER: Max number of Network Adapters. */
#ifndef MAX_NET_ADAPTER
#define MAX_NET_ADAPTER                1
#endif

/* MAX_UDP: Max nb of UDP controllers. */
#ifndef MAX_UDP
#define MAX_UDP                4
#endif

/* MAX_TCP: Max nb of TCP controllers. */
#ifndef MAX_TCP
#define MAX_TCP                20
#endif


/* MAX_TCP_SEG: Max nb of outgoing segments per TCP controller. */
#ifndef MAX_TCP_SEG
#define MAX_TCP_SEG                10
#endif

/* ---------- ARP options ---------- */

/*Max nb of hardware address IP address pairs cached.*/
#ifndef ARP_TABLE_SIZE
#define ARP_TABLE_SIZE                  10
#endif


/* ---------- TCP options ---------- */

#ifndef TCP_WND
#define TCP_WND                         16 * 1024
#endif

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS                         1460
#endif


/* ---------- Debug options ---------- */

//#define T_DEBUG

#if defined(T_DEBUG)
#define NETIF_DEBUG             DBG_ON
#define ETHARP_DEBUG            DBG_ON
#define IP_DEBUG                DBG_ON
#define ICMP_DEBUG              DBG_ON
#define UDP_DEBUG               DBG_ON
#define TCP_DEBUG               DBG_ON
#endif

#endif /* __DEFAULT_OPTIONS_H__ */



