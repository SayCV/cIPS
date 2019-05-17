#ident "@(#) $Id: main.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \mainpage An example of cIPS on Xilinx
<p>
<h2>Introduction</h2>
This webserver runs on the Xilinx SP3E1600E starter kit.<br>
It uses:
<ul>
<li>Microblaze in standalone mode.</li>
<li>The emaclite device driver.</li>
</ul>
It has also a TCP echo server on port#5009, an UDP one on port#5010, a UDP streaming towards the host and a periodic
PING to the host.
<h2>Instructions</h2>
<ul>
<li>Build the project (Xilinx C project).</li>
<li>Adapt the project to your hardware (modify hw_mapping.h).</li>
<li>Download your bitstream and to the target.</li>
<li>Plug the network cable.</li>
<li>Ping 10.2.222.222</li>
</ul>
<h2>TCP echo server and an UDP echo server</h2>
There is a client application to test the echo servers
<ul>
<li>Look for "cips_echo.exe".</li>
<li>Run "cips_echo.exe" (it is safe!). cips_echo.exe is directly linked to the "jar" file and the lib directory so do not move it. FYI, the executable has been generated with JSMOOTH.</li>
<li>Just follow the GUI!</li>
</ul>
<h2>UDP stream to the host and periodic PING to the host</h2>
The "connection_manager.c" file gives an example of how the Xilinx board can PING an host or how to open a connectio to a host server.
<b> The default host IP address is "10.2.2.100" (see connection_manager.c). Please, adapt the project to your IP address and mask. </b>
</p>
<p>
\author Jean-Marc David
</p>
<p>cIPS Release Tag: $Name: CIPS_EXAMPLE_03_006 $</p>
<p>file version: $Id: main.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $</p>

* \namespace main
* \file main.c 
* \brief Entry point to the application. <br> 
* \note 
***************************************************/

/* libc Includes */

/* Xilinx Includes */
#ifdef __XMK__
#include "xmk.h" //for xilkernel_main
#endif //__XMK__
#include "xil_cache.h"
#include "mb_interface.h"

/* C Library Includes */

/* Application Includes */
#include "debug_trace.h"
#include "application.h"
#include "hw_mapping.h"
 
/* External Declarations */

/* Internal Declarations */

/*!
 * Function name: clearScreen
 * \return nothing
 * \brief  Prints 24 newline characters to clear the 
 * display, assuming a standard 80x24 display
 * *******************************************************************/
void clearScreen(void)
{
  DT_TRACE(TRC_ON|TRC_MASK_LEVEL,("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\r\n"));
}

/*!
* Function name: main
* \return : 0 
********************************************************************/ 
int main(void)
{
#ifdef __PPC__
    Xil_ICacheEnableRegion(CACHEABLE_REGION_MASK);
    Xil_DCacheEnableRegion(CACHEABLE_REGION_MASK);
#elif __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
    Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
    Xil_DCacheEnable();
#endif
#endif

  microblaze_enable_interrupts();

  // Clear the screen to get rid of anything that may initially have
  // been displayed on the user's terminal
  clearScreen();

#ifdef __XMK__
  // Launch XMK
  xilkernel_main();
#else
  (void)wscm_start(NULL);
#endif //__XMK__

  //Disable cache and reinitialize it so that other applications can be run with no problems
    Xil_DCacheDisable();
    Xil_ICacheDisable();

  return 0;
}

#ifdef __XMK__
/*!
* Function name: starting_thread
* \return : NULL
* \param arg : nothing
* \brief  Starts the Webserver thread.
* \note
* *******************************************************************/  
void* starting_thread(void* arg)
{

  (void)wscm_start(NULL);

  return NULL;
}
#endif //__XMK__



