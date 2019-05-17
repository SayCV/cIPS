#ident "@(#) $Id: cips.h,v 1.5 2010/06/25 14:14:30 jdavid Exp $"
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
* \namespace cips
* \file cips.h
* \brief  Facade file of the cIPS library. <br>
* http://sourceforge.net/projects/cipsuite/ <br>
***************************************************/

#ifndef __CIP_H__
#define __CIPS_H__

#include "basic_c_types.h"
#include "nw_protocols.h"
#include "inet.h"
#include "tcp.h"
#include "udp.h"
#include "ip.h"
#include "err.h"
#include "netif.h"


#endif /* __CIPS_H__ */

/*! \mainpage cIPS 
<br> http://sourceforge.net/projects/cipsuite/
<h2>1. Abstract</h2>
<ul>
<li> cIPS stands for "compact Internet Protocol Suite" or "IP stack in C language". I was
looking for a short meaningful name.</li>
<li> <b>cIPS is a robust and fast TCP-IP library for embedded applications without an Operating System.</b><br>
The motivation of this project was to implement a fast and reliable TCP-IP stack
that could handle several network adapters on an embedded system.</li>
<li> cIPS is entirely abstract from any platform and therefore does not contain
any device drivers. It is some pure C code (to convince yourself, open
the dev-C++ project provided or use the Makefile to compile).</li>
</ul>

<h2>2. Features</h2>

<ul>
<li> Supports ARP, IP (v4), TCP and a subset of ICMP (ping). A subset of HTTP is
 implemented in a web server example but it is outside the library.</li>
<li> Supports several network adapters.</li>
<li> The maximum number of connections is configurable.</li>
<li> The traces are readable and are an option. However, there is also an error report strategy (see 3.2 Error reporting)</li>
<li> Data are separated from the processing.</li>
<li> Does not perform any dynamic memory allocation.</li>
<li> Fully tested on a big-endian platform. However swapping macros for
little-endian platforms are presents.</li>
</ul>

<h2>3. Principle of the cIPS library</h2>

When a Ethernet frame comes, the device driver triggers an ISR called
netif_ISR(). To keep that ISR short, it places the frame into a pool of frames.
After, netif_dispatch() is in charge of scanning the pool and demultiplexing the
frames is any.
<table class="image">
<tr><td><img src="../../principle.jpg" border="1"></td></tr>
<tr><td class="caption">Principle of the cIPS engine</td></tr>
</table>
<h3>3.1 Integration into your application</h3>
The library via netif_dispatch(), needs to be called repeatedly by the Application. In my configuration without
operating system, I integrated it into a while loop where I do many other things.
\code
while(1)
{
    err = netif_dispatch(netif_adapter);
    if(err) {printf(("%s \r\n", get_last_stack_error( netif_adapter, err)));}
    ....
}
\endcode
Example of integration:
<A HREF="../../example/web_server/connection_manager/connection_manager.c">connection_manager.c</A>,
<A HREF="../../example/web_server/connection_manager/main.c">main.c</A>


<h3>3.2 Error reporting</h3>
In the flow of demulteplexing and processing, all functions return an error code 
and the last error if any is parsed by get_last_stack_error().
The possible errors are listed in err.h and are most likely an alteration of the incoming frame
( wrong check sum ) or a resource limit reached. 
<h3>3.3 Several adapters.</h3>
Data are private to an adapter. So if you need two adapters, just declare two network adapter instances. 
\code
  NETIF_T* netif_adapter[2];

  netif_adapter[J1] = na_new(mac_address1, ip_address1, ...
  netif_adapter[J2] = na_new(mac_address2, ip_address2, ...

  while(1)
  {
    err = netif_dispatch(netif_adapter[EU_J2]);
    if(err) {DT_ERROR(("%s \r\n", get_last_stack_error( netif_adapter[EU_J2], err)));}

    err = netif_dispatch(netif_adapter[EU_J1]);
    if(err) {DT_ERROR(("%s \r\n", get_last_stack_error( netif_adapter[EU_J1], err)));}

    ...
  }
\endcode
<h2>4. Optimization</h2>
One of my requirements was to support two network adapters but another important one 
was to send data every millisecond via UDP between to embedded systems running at 50Mhz
and to support TCP on top.
<table class="image">
<tr><td><img src="../../configuration.jpg" border="2"></td></tr>
<tr><td class="caption">System configuration</td></tr>
</table>
<h3>4.1 UDP receiver</h3>
To process UDP frames as they come and as quickly as possible, UDP frames were
parsed and process in the ISR, the other ones (TCP, ARP, ICMP) were put into the FIFO.

To set that mode, set the optimized parameter of na_new() to TRUE. It will replace
netif_ISR() is replaced by netif_ISR_optimized().
 
<table class="image">
<tr><td><img src="../../realtime_udp.jpg" border="2"></td></tr>
<tr><td class="caption">UDP in the ISR</td></tr>
</table>

<h3>4.2 Sending UDP data</h3>
In a traditionnal case, a pointer to some data is passed as a parameter to udp_send()
and these data are copied to build an ethernet frame.

\code
MEASUREMENT_T measurement;
update(&measurement);
err = udp_send( udp_c, &measurement, sizeof(MEASUREMENT_T),  FALSE);
\endcode
<img src="../../udp_get_data_pointer.jpg" border="0">
In order to save that copy, it is possible to get a pointer to the data portion of the 
UDP frame and to work with that reference.
\code
MEASUREMENT_T* measurement = (MEASUREMENT_T*)udp_get_data_pointer(udp_cb);
update(measurement);
err = udp_send( udp_c, NULL, sizeof(MEASUREMENT_T),  FALSE)
\endcode

<h3>4.3 Sending the same amount of data to the same destination</h3>
Sometimes, an UDP connection is used to send repeatedly the same 
structure to the same destination. It would be inefficient to re-build the
ethernet frame from scratch when most of the field do not changed.
To address that case, set the "reuse" of udp_send() to TRUE.
\code
//err_t udp_send(UDP_T *udp_c, void* data, u32_t data_length, u32_t reuse)
err = udp_send( udp_c, NULL, sizeof(MEASUREMENT_T),  TRUE)
\endcode

<h3>4.6 Point to point</h3>
Application data are protected by check sums at three levels: Ethernet, IP and UDP.<br>
In a point to point configuration, the UDP check sum is redundant and its 
generation can be avoided to save some CPU processing.<br>
To inhibit the UDP check sum generation, set the point_to_point parameter of udp_new() to TRUE.

\code
  //UDP_T * udp_new( u32_t ipaddr, u16_t port, u32_t point_to_point);
  udp_cb = udp_new(ip_addr, port, TRUE);
\endcode
<h2>5. Device driver API requirements</h2>

cIPS is compatible with device drivers that respect the following interface:
<ul>
<li> u32_t driver_receive(void* pDriver_arg, u8_t *eth_frame);</li>
<li> u32_t driver_send(void* pDriver_arg, u8_t *eth_frame, u32_t byte_count);</li>
<li> void _ISR(void *network_adapater);</li>
</ul>
Otherwise an adaptation layer is needed.

Example of adaptation layer:
<A HREF="../../example/web_server/network_adapter/network_adapter.c">network_adapter.c</A>,
<A HREF="../../example/web_server/network_adapter/network_adapter.h">network_adapter.h</A>

<p>
\author Jean-Marc David <br> jmdavid1789<at>gmail.com
</p>
 <p>cIPS Release Tag: $Name: CIPS_03_007 $</p>
 <p>file version: $Id: cips.h,v 1.5 2010/06/25 14:14:30 jdavid Exp $</p>
 */

