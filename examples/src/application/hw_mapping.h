#ident "@(#) $Id: hw_mapping.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace  hm
* \file hw_mapping.h 
* \brief  Module Description: 
* Rename the hardware description for a better 
* understanding
***************************************************/

#ifndef _HW_MAPPING_
#define _HW_MAPPING_

/* Include automatically generated system headers */
#include "xparameters.h"

/* Redefinitions for generated device parameters from xparameters.h */
#define MAX_CACHE XPAR_MICROBLAZE_0_DCACHE_BYTE_SIZE //!<Maximum cache size
#ifndef XPAR_MICROBLAZE_0_DCACHE_BYTE_SIZE
#error "XPAR_MICROBLAZE_0_DCACHE_BYTE_SIZE is not defined in xparameters.h"
#endif

#define EMAC_INTERRUPT_ID1 XPAR_XPS_INTC_0_ETHERNET_MAC1_IP2INTC_IRPT_INTR
#ifndef XPAR_XPS_INTC_0_ETHERNET_MAC1_IP2INTC_IRPT_INTR
#error "XPAR_XPS_INTC_0_ETHERNET_MAC1_IP2INTC_IRPT_INTR is not defined in xparameters.h"
#endif

#define ETHERNET_MAC1 XPAR_ETHERNET_MAC1_DEVICE_ID
#ifndef XPAR_ETHERNET_MAC1_DEVICE_ID
#error "XPAR_ETHERNET_MAC1_DEVICE_ID is not defined in xparameters.h"
#endif

#define INTERRUPT_CONTROLLER_BASE_ADDR XPAR_XPS_INTC_0_BASEADDR
#ifndef XPAR_XPS_INTC_0_BASEADDR
#error "XPAR_XPS_INTC_0_BASEADDR is not defined in xparameters.h"
#endif

#define EMAC_CONTROLLER_MASK XPAR_ETHERNET_MAC1_IP2INTC_IRPT_MASK
#ifndef XPAR_ETHERNET_MAC1_IP2INTC_IRPT_MASK
#error "XPAR_ETHERNET_MAC1_IP2INTC_IRPT_MASK is not defined in xparameters.h"
#endif

/*Timer*/
#define TIMER_BASE_ADDR XPAR_XPS_TIMER_1_BASEADDR //!<Timer number 2.
#ifndef XPAR_XPS_TIMER_1_BASEADDR
#error "XPAR_XPS_TIMER_1_BASEADDR is not defined in xparameters.h"
#endif

#define TIMER_INTERRUPT_ID XPAR_XPS_INTC_0_XPS_TIMER_1_INTERRUPT_INTR //!<Timer number 2.
#ifndef XPAR_XPS_INTC_0_XPS_TIMER_1_INTERRUPT_INTR
#error "XPAR_XPS_INTC_0_XPS_TIMER_1_INTERRUPT_INTR is not defined in xparameters.h"
#endif

#define TIMER_INTERRUPT_MASK XPAR_XPS_TIMER_1_INTERRUPT_MASK
#ifndef XPAR_XPS_TIMER_1_INTERRUPT_MASK
#error "XPAR_XPS_TIMER_1_INTERRUPT_MASK is not defined in xparameters.h"
#endif

#endif //_HW_MAPPING_
