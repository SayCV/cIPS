#ident "@(#) $Id: application.c,v 1.1 2012/12/06 16:20:57 jdavid Exp $"
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
* \file application.c
* \brief This module integrates the cIPS stack and a webserver <br>
* There are several possible configurations: <br>
* - No OS (Standalone) + timer. <br>
* - Xilkernel (_XMK_) + timer. <br>
***************************************************/

/* libc Includes */

/* TCP-IP Includes */
#include "cips.h"

/* Xilinx Includes */
#include "xtmrctr.h" //for XTmrCtr_xxx, XTC_CSR_xxx  
#include "xintc_l.h" //for (__XMK__,XIntc_RegisterHandler) or (standalone, XIntc_mMasterEnable)

//for timer interrupt
#include <xtmrctr.h> //for XTmrCtr_xxx, XTC_CSR_xxx
#include <xtmrctr_l.h>
#include <xintc_l.h> //for (__XMK__,XIntc_RegisterHandler) or (standalone, XIntc_mMasterEnable)
#ifdef __XMK__
#include <xil_macroback.h>
#include <sys/intr.h> //for int_id_t
#endif //__XMK__

/* Application Includes */
#include "basic_types.h"
#include "debug_trace.h"
#include "network_adapter.h"
#include "hw_mapping.h"
#include "timer.h"
#include "application.h"

/* External Declarations */


/* Internal Declarations */
static const s8_t ADAPTER_NAME[] = "J1"; //!< "J1" is the name on the Print Board.
#define TIMER_PERIOD ((const u32_t)TCP_TIMER_PERIOD) //!< The application uses a hardware timer to get ticks at 500ms interval. Note that CIPS must call tcp_timer() every 500ms.
static const u8_t TIMER_COUNTER_INDEX = 0; //!<There are XTC_DEVICE_TIMER_COUNT timer counters (or channels) in a hardware timer (TIMER_2). I choosed the first one.
static const u32_t CHECK_CONNECTION_PERIOD = 60; //!< In seconds. It is indirectly the nb of tcp_timer() before calling tcp_check_connection.
//!Nb of ticks to get the 500ms required by the TCP protocol (in this case, it equals 1!)
#define TCP_TICKS          50 //!< The timer ticks every 10 ms. 50 x 10 ms = 500 ms
#define UDP_CONNECT_TICKS  500 //!< The application sends a ping to the peer device every 10s.
#define PING_PEER_DEVICE_TICKS    1000 //!< The timer ticks every 10 ms. 1000 x 10 ms = 10000 ms = 10s. The application sends a ping to the peer device every 10s.

//!Define the structure passed as an argument to the tcp callbacks.
typedef struct {
  //empty for that example.
} WSCM_TCP_ARG_T;

/*!The application decides how often it wants to check whether the tcp_c's connection <br>
has been inactive. The application specifies the period in second . CIPS deals with <br>
tick numbers of 500ms. CHECK_CONNECTION_PERIOD_TO_NUMBER() does the convertion from <br>
seconds to ticks.*/
#define CHECK_CONNECTION_PERIOD_TO_NUMBER(check_connection_period) ( (check_connection_period) * 1000 / TCP_TIMER_PERIOD)

// These following prototypes are the callbacks in the format expected by cIPS.
static err_t echo_udp_recv(void *arg, UDP_T *udp_cb, void* pdata, u32_t data_length);
static err_t echo_tcp_accept(void *arg, TCP_T *new_tcp_c);
static err_t echo_tcp_recv(void *arg, TCP_T *tcp_c, void* data, u32_t data_length);
static err_t echo_tcp_check_connection(void *arg, TCP_T *tcp_c);
//Example static err_t tcp_client_recv(void *arg, TCP_T *pcb, void* data, u32_t data_length);
//static err_t tcp_client_check_connection(void* arg, TCP_T* pcb);

// Application prototypes
static void wscm_display_configuration(const s8_t* adapter_name, const s8_t* const mac_address, const s8_t* const ip_address, const s8_t* const subnet, const s8_t* const gateway);

/*!
 * Function name: wscm_display_configuration
 * \return nothing
 * \param adapter_name : [in] Adapter name to identify J1 or J2 on the PCB.
 * \param mac_address : [in] mac_address
 * \param ip_address : [in] ip_address
 * \param subnet : [in] subnet
 * \param gateway : [in] gateway
 * \brief Displays all of the currently set configuration
 * options for a network adapter on a hyperterminal
 * *******************************************************************/
static void wscm_display_configuration(const s8_t* adapter_name,
  const s8_t* const mac_address,
  const s8_t* const ip_address,
  const s8_t* const subnet,
  const s8_t* const gateway)
{
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("Adapter    : %s\r\n",adapter_name));
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("MAC Address: %s\r\n",mac_address));
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("IP Address : %s\r\n",ip_address));
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("Subnet Mask: %s\r\n",subnet));
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("Gateway    : %s\r\n",gateway));
}


/*!
 * Function name: echo_tcp_check_connection
 * \return ERR_OK
 * \param arg : [in/out] anything the application needs. Not used here.
 * \param tcp_c : [in] Protocol control Block of the TCP connection.
 * \brief Close the socket if no activity for too long.
 * \note If a TCP connection connects a peer device to cIPS and if there is no
 * traffic between the two. Either the connection is broken or it is the
 * normal state of operation between the peer device and the application not to
 * exchange messages. cIPS cannot make the difference between the two 
 * situations but the application can. So cIPS counts the inactivity time 
 * and notifies the application (through the echo_tcp_check_connection() 
 * callback). The application can decide to close the connection, to test
 * the connection or to do nothing.
 * *******************************************************************/
static err_t echo_tcp_check_connection(void *arg, TCP_T *tcp_c)
{
  err_t err = ERR_OK;
  // Close the tcp connection
  (void)tcp_close(tcp_c);
  return err;
}


/*!
 * Function name: wscm_tcp_accept
 * \return ERR_OK.
 * \param arg : [in] structure containing a reference to the socket of interest.
 * \param new_tcp_c : [in] child socket of the server.
 * \brief Specify the function that a child connection calls when a 
 * server creates it. A peer device client connects to a cIPS TCP server. 
 * The TCP server creates a child controller. But the child is not 
 * connected to the application. echo_tcp_accept() links the child to the 
 * application. The application configures the child connection.
 * To do so, it will call tcp_recv(), tcp_check_connection() and 
 * tcp_closed() in echo_tcp_accept().
 * *******************************************************************/
static err_t echo_tcp_accept(void *arg, TCP_T *new_tcp_c)
{
  err_t err = ERR_OK;
  (void)tcp_recv(new_tcp_c, echo_tcp_recv);
  (void)tcp_check_connection(new_tcp_c, echo_tcp_check_connection, CHECK_CONNECTION_PERIOD_TO_NUMBER( CHECK_CONNECTION_PERIOD ));
  return err;
}


/*!
 * Function name: echo_tcp_recv
 * \return "tcp_write" error code that will be caught by netif_dispatch().
 * \param arg : [in/out] anything the application needs. Not used here.
 * \param tcp_c : [in] TCP context block.
 * \param data: [in] buffer received.
 * \param data_length: [in] length of "data".
 * \brief Echo a TCP frame.
 * *******************************************************************/
static err_t echo_tcp_recv(void *arg, TCP_T *tcp_c, void* data, u32_t data_length)
{
  err_t err = tcp_write(tcp_c, data, data_length); //Echo the incoming data
  return err;
}

/*!
 * Function name: echo_udp_recv
 * \return "udp_send" error code that will be caught by netif_dispatch().
 * \param arg : [in/out] Any application data.
 * \param udp_cb : [in] UDP context block.
 * \param data : [in] buffer of data.
 * \param data_length : [in] length of pdata.
 * \brief  Echo an UDP frame.
 * *******************************************************************/
static err_t echo_udp_recv(void *arg, UDP_T *udp_cb, void* data, u32_t data_length)
{
  err_t err = ERR_OK;
  err = udp_send(udp_cb, data, data_length,FALSE);  //Echo the incoming data
  return err;
}

/*!
 * Function name: wscm_start
 * \return NULL
 * \param arg : [in] not used
 * \brief <br>
 * - Initialization.<br>
 * - Setup the network adapter.<br>
 * - Display the configuration options.<br>
 * - Create a TCP connection.<br>
 * - Start the timer.<br>
 * - Start the infinite processing loop.
 * *******************************************************************/
void* wscm_start(void* arg)
{
  err_t err;
  u32_t host_ip_addr;
  u32_t peer_device_ip_addr;
  NETIF_T* netif_adapter;
  TIMER_T timer;
  bool_t tick = FALSE;
  const s8_t peer_device_ip_address[] = "10.2.2.100"; //!< IP address of the machine (most likely your PC) communicating with the web server application.
  const s8_t host_ip_address[] = "10.2.222.222";
  const s8_t host_mac_address[] = "00-19-33-65-55-05";
  const s8_t host_gateway[] = "0.0.0.0"; //no gateway
  const s8_t host_netmask[] = "255.255.255.0";
  
  const u16_t tcp_echo_port = 5009;
  const u16_t udp_echo_port = 5010;
  const u16_t app_udp_stream_port = 5011;
  const u16_t peer_device_udp_stream_port = 5033;
  TCP_T* echo_tcp_cb= NULL;
//  TCP_T* tcp_client_cb= NULL; //Example of a TCP client to a peer device server.
  UDP_T* echo_udp_cb= NULL;
  UDP_T *udp_stream_to_peer_device_cb = NULL;
  u32_t tcp_counter = 0;
  u32_t udp_down_stream_counter = 0;
  u32_t ping_peer_device_counter = 0;
  u32_t timer_counter = 0; //absolute number of counts reported by the timer


  err = inet_addr(host_ip_address, &host_ip_addr);
  err = inet_addr(peer_device_ip_address, &peer_device_ip_addr);
  
  // Init the network_adapter module
  (void)na_init();

  // Setup the network adapter
#ifdef __XMK__  
  netif_adapter = na_new(host_mac_address, host_ip_address, host_netmask, host_gateway,
  ETHERNET_MAC1 ,EMAC_INTERRUPT_ID1, FALSE, ADAPTER_NAME, &err);
#else //__XMK__
  netif_adapter = na_new(host_mac_address, host_ip_address, host_netmask, host_gateway,
  ETHERNET_MAC1, EMAC_INTERRUPT_ID1, INTERRUPT_CONTROLLER_BASE_ADDR, FALSE, ADAPTER_NAME, &err);
#endif //__XMK__
  DT_ASSERT(WSCM_DEBUG, netif_adapter != NULL);

  // Display the configuration options on a hyperterminal
  (void)wscm_display_configuration(ADAPTER_NAME ,host_mac_address,host_ip_address,host_netmask,host_gateway);
  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("Webserver port : 80\r\nTCP echo port : %d\r\nUDP echo port : %d\r\n\r\n",tcp_echo_port,udp_echo_port));

  // Create a TCP connection
  echo_tcp_cb = tcp_new( host_ip_addr, tcp_echo_port, &err);
  DT_ASSERT(WSCM_DEBUG, !err);

  (void)tcp_arg(echo_tcp_cb, NULL);
  (void)tcp_accept( echo_tcp_cb, echo_tcp_accept);

  DT_TRACE(WSCM_DEBUG|TRC_LEVEL_INFO,("Listen for new TCP connection on port#%d\r\n",tcp_echo_port));
  err = tcp_listen( echo_tcp_cb);
  DT_ASSERT(WSCM_DEBUG,!err);

  // Create an UDP connection
  echo_udp_cb = udp_new( host_ip_addr, udp_echo_port, FALSE, &err);
  DT_ASSERT(WSCM_DEBUG, !err);
  (void)udp_recv(echo_udp_cb, echo_udp_recv, NULL);

  //Open a UDP connection to the peer device in order to send a stream of data to the peer device.
  udp_stream_to_peer_device_cb = udp_new(host_ip_addr, app_udp_stream_port, FALSE, &err);
  DT_ASSERT(WSCM_DEBUG, !err);

  //Example: Open a TCP client to the peer device server.
  //#define TCP_CLIENT_LOCAL_PORT 5001
  //#define TCP_PEER_DEVICE_SERVER_PORT 5002
  //tcp_client_cb = tcp_new( host_ip_addr, TCP_CLIENT_LOCAL_PORT,&err);
  //(void)tcp_recv(tcp_client_cb, tcp_client_recv);
  //(void)tcp_check_connection(tcp_client_cb, tcp_client_check_connection, 10);
  //err = tcp_connect(tcp_client_cb, peer_device_ip_addr, TCP_PEER_DEVICE_SERVER_PORT, NULL);

  // Initialize the timer
  timer.base_address = TIMER_BASE_ADDR;
  timer.timer_number = TIMER_COUNTER_INDEX;
  (void)ti_init_capture_timer( TIMER_BASE_ADDR, TIMER_COUNTER_INDEX);

#ifndef __XMK__
  // Start the interrupt controller
  XIntc_MasterEnable(INTERRUPT_CONTROLLER_BASE_ADDR);
  //Start the interrupt controller. All the interruption must be registered beforehand
  XIntc_EnableIntr(INTERRUPT_CONTROLLER_BASE_ADDR, EMAC_CONTROLLER_MASK);
#endif //__XMK__

  // Start the infinite processing loop
  while (1) {
    // An incoming ethernet frame is put into a FIFO buffer by an ISR. 
    // netif_dispatch retrieves the first one
    err = netif_dispatch(netif_adapter);
    if(err) {DT_ERROR(("%s \r\n", get_last_stack_error( netif_adapter, err)));}

    // Get time and periodic action to perform
    (void)ti_get_timer_event(&timer, &timer_counter, &tick);

    if(tick){ //after 10 milliseconds (or more)
      // Increment all the counters.
      tcp_counter++;
      udp_down_stream_counter++;
      ping_peer_device_counter++;
      // The timer is decimated
      if (tcp_counter >= TCP_TICKS){
        tcp_counter = 0; //1 makes the condition TRUE after TCP_TICKS counts exactly.
        err = tcp_timer(netif_adapter); //If in another application, TCP is not used then tcp_timer() can be removed.
        if(err) {
          DT_ERROR(("%s \r\n", get_last_stack_error(netif_adapter, err)));
        }
      } else if (udp_down_stream_counter >= UDP_CONNECT_TICKS){
        #define UDP_STREAM_LENGTH 32
        static u8_t udp_data[UDP_STREAM_LENGTH] = "0; cIPS UDP ; InternetProtoSuite"; //Check that message on Wireshark.

        udp_down_stream_counter = 0;
        //Note: if you have a TCP client instead of a UDP one then connect to the peer device with tcp_connect().
        err = udp_connect(udp_stream_to_peer_device_cb, peer_device_ip_addr, peer_device_udp_stream_port);
        if(!err){
          err = udp_send(udp_stream_to_peer_device_cb,udp_data, UDP_STREAM_LENGTH, FALSE);
          if (err){
            DT_ERROR(("%s \r\n", get_last_stack_error( udp_stream_to_peer_device_cb->netif, err)));
          }
        }
        //Increment the first character of the UDP payload
        if(udp_data[0] != '9'){
          udp_data[0]++;
        } else {
          udp_data[0] = '0';
        }
        //Example of a TCP client
//          if( tcp_client_cb->state == CLOSED){
//          //The application re-connects with the same local port number and with the same initiated tcp connector.
//          //Note: If cIPS does know the peer device yet, it uses tcp_connect() to resolve the (IP,MAC) pair (ARP request).
//          //tcp_connect() sends an ARP request until it is resolved. So the peer device does not exit, you will see a
//          //lot of ARP request on the network. A tip would be to call tcp_connect() periodically with a low period.
//            err = tcp_connect(tcp_client_cb, peer_device_ip_addr, TCP_PEER_DEVICE_SERVER_PORT, NULL);
//OR        //The application re-connects with a new local port number defined by cIPS
//OR          err= tcp_delete(tcp_client_cb);
//OR          if(!err){
//OR             tcp_client_cb = tcp_new( peer_device_ip_addr, 0,&err); //0 means local port number defined by cIPS. It re-configures the tcp connector.
//OR             (void)tcp_recv(tcp_client_cb, tcp_client_recv);
//OR             (void)tcp_check_connection(tcp_client_cb, tcp_client_check_connection, 10);
//OR             err = tcp_connect(tcp_client_cb, peer_device_ip_addr, TCP_PEER_DEVICE_SERVER_PORT, NULL);
//OR           }
//          }
      } else if (ping_peer_device_counter >= PING_PEER_DEVICE_TICKS){
        //This application pings the peer device periodically
        #define PING_DATA_LENGTH 32
        static u8_t ping_data[PING_DATA_LENGTH] = "0; cIPS ping; InternetProtoSuite"; //Check that message on Wireshark.
        ping_peer_device_counter = 0;
        err = ip_icmp_ping(netif_adapter, peer_device_ip_addr, ping_data, PING_DATA_LENGTH);
        //Increment the first character of the PING payload
        if(ping_data[0] != '9'){
          ping_data[0]++;
        } else {
          ping_data[0] = '0';
        }

      }
    }
  }
  // This line is never reached. It is an example
  (void)netif_delete(netif_adapter);

  return NULL;
}

/*!
 * Function name: tcp_client_recv
 * \return ERR_OK.
 * \param arg : [in/out] anything the application needs. Not used here.
 * \param tcp_c : [in] TCP context block.
 * \param data : [in]
 * \param data_length : [in]
 * \brief When the TCP frame arrives, netif_dispatch() calls tcp_client_recv()
 * so that the application can process the frame.
 * *******************************************************************/
//static err_t tcp_client_recv(void *arg, TCP_T *tcp_c, void* data, u32_t data_length)
//{
//  err_t err = ERR_OK;
//  xil_printf("%s#%ld The peer device server sent a message to the application TCP client\r\n",__func__,__LINE__);
//  return err;
//}

/*!
 * Function name: tcp_client_check_connection
 * \return ERR_OK.
 * \param arg : [in/out] anything the application needs. Not used here.
 * \param tcp_c : [in] TCP context block.
 * \brief The connection has been inactive, the application can choose
 * between closing it or leaving it alive.
 * *******************************************************************/
//static err_t tcp_client_check_connection(void* arg, TCP_T* tcp_c)
//{
//  err_t err = ERR_OK;
//  //The application chooses to close the connection if it is in established state
//  if (tcp_c->state == ESTABLISHED) {
//    (void)tcp_close(tcp_c);
//  }
//  return err;
//}


