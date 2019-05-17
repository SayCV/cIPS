#ident "@(#) $Id: tcp.h,v 1.1 2012/10/12 18:59:09 jdavid Exp $"
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
* \namespace tcp
* \file tcp.h
* \brief TCP layer. The reference is the RFC793.
* The following code implement the "TCP Connection State Diagram"
* of the RFC793.
***************************************************/

#ifndef __TCP_H__
#define __TCP_H__

#include "arch.h"

#ifndef TCP_TIMER_PERIOD
#define TCP_TIMER_PERIOD  500  /*TCP timer period in milliseconds. */
#endif

#define TCP_FIN_WAIT_TIMEOUT 4000 /* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT 10000 /* milliseconds */
#define TCP_RETRANSMISSION_TIMEOUT 3000 /* milliseconds */

//! The application can configure a connection with the following options
//! and tcp_options().
 typedef enum{
  tcp_no_options = 0,
  tcp_delay_ack_reply = 1, //Bit 0: ACK delayed (set), immediate otherwise.
  tcp_option1 = (1<<1), //Bit 1: for future usage.
  tcp_option2 = (1<<2), //Bit 2: for future usage.
} TCP_OPTIONS_T;

/*!TCP states*/
enum tcp_state {
  CLOSED      = 0,
  LISTEN      = 1,
  SYN_SENT    = 2,
  SYN_RCVD    = 3,
  ESTABLISHED = 4,
  FIN_WAIT_1  = 5,
  FIN_WAIT_2  = 6,
  CLOSE_WAIT  = 7,
  CLOSING     = 8,
  LAST_ACK    = 9,
  TIME_WAIT   = 10
};

/*!Command received by the TCP state machine and coming from the user (or application) */
typedef enum{
  TCP_USER_PASSIVE_OPEN,
  TCP_USER_ACTIVE_OPEN,
  TCP_USER_SEND,
  TCP_USER_CLOSE
} TCP_USER_COMMAND;

/*!TCP must keep track of the frames it sends. So TCP has a pool of sending segments with a state
TCP_SEG_UNUSED: the segment is free.
TCP_SEG_UNSENT: the segment contains a frame that has not been sent yet.
TCP_SEG_UNACKED: the segment contains a frame that has been sent but not acknowledged yet.*/
typedef enum {
  TCP_SEG_UNUSED = 0,
  TCP_SEG_UNSENT = 1,
  TCP_SEG_UNACKED = 2,
  TCP_SEG_NB
} seg_state;

struct NETIF_S;

//< This structure is used to repressent TCP segments when queued.
typedef struct TCP_SENDING_SEG_S {
  seg_state state; //!< TCP_SEG_UNUSED, TCP_SEG_UNSENT or TCP_SEG_UNACKED.
  u32_t ack_no; //!< acknowlegement number expected when an ACK is received.
  u8_t frame[NETWORK_MTU]; //!< buffer containing the entire ethernet frame.
  bool_t frame_initialized; //!< Flag indicating whether the constant fields have been set in "frame" (TRUE if set).
  u16_t len; //!< the Ethernet length of this segment.
  TCP_HEADER_T *tcphdr; //!< the TCP header.
  u32_t retransmission_timer_slice; //!< retransmission timer slice for this segment. The application calls tcp_timer() every 500ms but the TCP retransmission is 3s. retransmission_timer_slice is a counter adapting one period (500ms) to the other (3s). tcp_timer() increments retransmission_timer_slice until it reaches 3s.
} TCP_SENDING_SEG_T;

//< TCP control
typedef struct TCP_S {
  u32_t local_ip; //!<CIPS ip address
  u32_t remote_ip; //!<The ip address of the peer device that cIPS communicates with.
  u16_t local_port; //!<CIPS TCP port
  u16_t remote_port; //!<The TCP port of the peer device that cIPS communicates with.
  struct TCP_S *next; //!< for the linked list
  struct NETIF_S *netif; //!< network interface for this packet
  enum tcp_state state; //!< TCP state. See "TCP Connection State Diagram" of the RFC793.
  TCP_SENDING_SEG_T control_segment; //!< buffer containing the entire ethernet frame used to send an ACK
  //! remote_ACK_counter: The peer device sends a TCP frame to cIPS, then cIPS must 
  //! acknowledge it. The acknowledgment is not always right away. It can be 
  //! delayed and multiplexed with the next outgoing data (tcp_write()).
  //! "remote_ACK_counter" is associated with tcp_options() and tcp_ack()
  //! 0 means that cIPS has no acknowledgment to send. Greater than zero represents 
  //! the amount of ACK received since the last sent. "remote_ACK_counter" acknowledges "remote_seqno".
  u32_t remote_ACK_counter;
  void *callback_arg;
  // receiver variables
  u32_t remote_seqno; //!< next seqno expected
  u16_t remote_wnd; //!< caller window
  u32_t remote_mss; //!< caller mss
  // Timers
  u32_t timer; //!< counter to manage the time out.
  u32_t counter_of_500ms; //!< The application calls tcp_timer() every 500ms and increments "counter_of_500ms" until "nb_of_500ms". Then the application checks whether the connection has been inactive.
  u32_t nb_of_500ms; //!< The application decides how often it wants to check whether the tcp_c's connection has been inactive. The application checks the inactivity every "nb_of_500ms x 500" ms.
  
  u32_t local_mss; //!< maximum segment size
  u16_t local_wnd; //!< local window
  u32_t retransmission_time_out; //!< The retransmission time out is a counter representing time. It is the maximum amount of fraction of TCP_TIMER_PERIOD to reach TCP_RETRANSMISSION_TIMEOUT.
  u32_t local_seqno; //!< Sequence number of next byte to be buffered.
  u32_t seg_nb[TCP_SEG_NB]; //!<Number of segments of "segment[MAX_TCP_SEG]" in the state "UNUSED", "UNSENT","UNACKED".
  TCP_SENDING_SEG_T segment[MAX_TCP_SEG]; //!< buffer holding the outgoing frames to the peer device.
  u32_t last_ack_no; //!< cIPS acknowleges segments with an ack_no number lower than the one received. But the received ack_no rolls over when it is bigger than 0xFFFFFFFF. last_ack_no handles the rollover situation.
  u8_t incoming_stream[TCP_MSS * MAX_TCP_SEG]; //!< buffer holding incoming stream from the peer device.
  u8_t* stream_position; //!<End of incoming stream in "incoming_stream".
  u32_t stream_sequence; //!<Nb of incoming packets related to a stream.
  u32_t seqno_ori; //!<initial seq no of a stream.
  TCP_OPTIONS_T options; //! The application can configure a connection with options (see TCP_OPTIONS_T)

  err_t (* recv)(void *arg, struct TCP_S *tcp_c, void* data, u32_t data_length);//!< Callback when data have been received
  err_t (* connect)(void *arg, struct TCP_S *tcp_c);//!< Callback when the socket has not contacted a server successfully.
  err_t (* accept)(void *arg, struct TCP_S *newtcp_c);//!< Callback when the socket accepts a client
  err_t (* periodic_connection_check)(void *arg, struct TCP_S *tcp_c);//!< Callback when no activity is detected for the socket for a certain amount of time
  err_t (* closed)(void *arg, struct TCP_S *tcp_c, err_t err);//!< Callback after closure.

  s32_t id;  //!< ID: Unused or used. Make the difference between the controllers available and the ones that the application is using.
  u32_t type;  //!< type: TCP_PERSISTENT or TCP_NON_PERSISTENT. See TCP_CATEGORY.
} TCP_T;

#ifdef __cplusplus
extern "C" {
#endif

// Application program's interface:

/*!
 * Function name: tcp_new
 * \return tcp_c: [out] the tcp_c created or NULL if no more controllers can be created.
 * \param ipaddr : [in] remote IP address to reach.
 * \param port : [in] remote port to reach.
 * \param err : [out] ERR_TCP_MEM if more than MAX_TCP tcp connection 
 * already exist, ERR_VAL if the IP@ is wrong, ERR_OK otherwise.
 * \brief Create a new TCP Protocol Control Block or TCB (see RFC793).
 * \note if the application creates a TCP client then it declares that
 * it is a client by calling tcp_connect() and it specifies the IP address 
 * and port number of the remote server in the tcp_connect() arguments.
 * if the application creates a TCP server then it declares that it is 
 * a server by calling tcp_accept() and then tcp_listen().
 * *******************************************************************/
TCP_T* tcp_new(u32_t ipaddr, u16_t port, err_t* err);

/*!
 * Function name: tcp_connect
 * \return ERR_OK, ERR_MAC_ADDR_UNKNOWN, ERR_VAL
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param ipaddr : [in] remote IP address to reach (the address of the peer device).
 * \param port : [in] remote port to reach (the port number of the peer device).
 * \param connect : [in] application callback. See the note.
 * \brief The application creates a TCP client, tcp_connect() connects 
 * the client to the peer's server.
 * \note <br>
 * The controller is a TCP client. It is in the CLOSED state but 
 * the application has called tcp_connect() in order to reach a 
 * peer server. Reaching the peer server necessitates two steps:
 * 1. to resolve the (IP,MAC) link. This step can take several 
 * ARP calls. It can last for ever if the server does not exist.
 * 2. to exchange the TCP connect message.
 * Potentially several times #1 and then #2 means that tcp_connect()
 * might be call several times.
 * If one call is not enough, the purpose of the callback is to call 
 * again tcp_connect().
 * It is a callback rather than an internal call so that the application
 * can decide what to do if the server does not exist (keep trying or time-out).
 * In addition, in the callback, the application can space out the 
 * tcp_connect() to more than the tcp_timer() period.
 * Note 1: if the server disconnect the client, then the client 
 * goes to CLOSED state. If the tcp_c->connect callback is defined 
 * then tcp_timer() calls it and the client re-connects.
 * Note 2: the content of the callback can be:
 * err = tcp_connect(tcp_c, tcp_c->remote_ip, tcp_c->remote_port, tcp_c->connect);
 * *******************************************************************/
err_t tcp_connect (TCP_T *tcp_c, u32_t ipaddr, u16_t port, err_t (* connect)(void *arg, TCP_T *tcp_c));

/*!
 * Function name: tcp_listen
 * \return the tcp_c in argument.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \brief The application creates a TCP server.
 * *******************************************************************/
err_t tcp_listen (TCP_T *tcp_c);

/*!
 * Function name: tcp_close
 * \return ERR_OK.
 * \param tcp_c: [in/out]connection of interest.
 * \brief Send a close message.
 * *******************************************************************/
err_t tcp_close (TCP_T *tcp_c);

/*!
 * Function name: tcp_write
 * \return ERR_OK, 
 * ERR_SEG_MEM if the application sends too much data in one call or 
 * ERR_APP if the application uses tcp_write() when it is not connected.
 * \param tcp_c: [in/out]connection of interest.
 * \param app_data: [in]Application data in "unsigned char".
 * \param app_len: [in]Application data length in bytes.
 * \brief Write some data to a TCP connection.
 * \note send the first elt (or multiplex with an ACK) and then put 
 * the other segments in the unsent queue. The ACK of the first 
 * segment is going to trigger the sending of the second and so on.
 * *******************************************************************/
err_t tcp_write (TCP_T *tcp_c, const void *app_data, u32_t app_len);

/*!
 * Function name: tcp_arg
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param arg : [in] argument structure.
 * \brief CIPS communicates with the application through callbacks. 
 * The application does the processing in the callbacks.
 * The application might need some of its internal data to do the 
 * processing. "arg" joins the internal data with the callback. So
 * the application uses "arg" to pass the data it needs in the callbacks
 * *******************************************************************/
void tcp_arg (TCP_T *tcp_c, void *arg);

/*!
 * Function name: tcp_accept
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param accept : [in] accept callback.
 * \brief Specify the function that a child connection calls when a Server creates it.
 * A peer client connects to a cIPS TCP server. The TCP server creates a child controller.
 * But the child is not connected to the application. tcp_accept() links 
 * the child to the application. The application configures the child connection.
 * To do so, it will call tcp_recv(), tcp_check_connection() and tcp_closed() in tcp_accept().
 * *******************************************************************/
void tcp_accept (TCP_T *tcp_c, err_t (* accept)(void *arg, TCP_T *newtcp_c));

/*!
 * Function name: tcp_recv
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param recv : [in] Received callback.
 * \brief The peer device sends data to cIPS. CIPS must forward them to the 
 * application. tcp_recv() makes the link between cIPS and the 
 * application. It specifies which application function cIPS should 
 * call when it receives TCP data for this tcp_c.
 * *******************************************************************/
void tcp_recv (TCP_T *tcp_c, err_t (* recv)(void *arg, TCP_T *tcp_c, void* data, u32_t data_length));

/*!
 * Function name: tcp_check_connection
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param periodic_connection_check : [in] periodic connection check callback. 
 * If set to NULL then the TCP core check whether the connection is alive.
 * \param nb_of_500ms : [in] It is a counter. The application must 
 * call tcp_timer() every 500ms. The application decides how often it 
 * wants to check whether the tcp_c's connection has been inactive.
 * The application checks the inactivity every "nb_of_500ms x 500" ms.
 * \param nb_of_500ms : [in] It is a counter. It is the number of tcp_timer() 
 * calls (or TCP_TIMER_PERIOD slices) before calling the 
 * application's periodic_connection_check()" callback.
 * \brief Set the function that the application uses to specify what to do if a connection has been inactive.
 * \note If a TCP connection connects a peer device to cIPS and if there is no traffic between the two.
 * Either the connection is broken or it is the normal state of operation between the peer device and the application not to exchange messages.
 * cIPS cannot make the difference between the two situations but the application can.
 * So cIPS counts the inactivity time and notifies the application (through the periodic_connection_check() callback).
 * The application can decide to close the connection, to test the connection or to do nothing.
 * *******************************************************************/
void tcp_check_connection(TCP_T *tcp_c, err_t (*periodic_connection_check)(void *arg, TCP_T *tcp_c), u32_t nb_of_500ms);

 /*!
 * Function name: tcp_closed
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param closed : [in] Closed callback.
 * \brief CIPS closes a connection. It notifies the application.
 * tcp_closed() sets the application function that cIPS uses to notify 
 * the application that the connection has just been closed.
 * *******************************************************************/
void tcp_closed (TCP_T *tcp_c, err_t (* closed)(void *arg, TCP_T *tcp_c, err_t err));

 /*!
 * Function name: tcp_abort
 * \return nothing.
 * \param tcp_c: [in/out]connection of interest.
 * \brief Send a RST and close the tcp_c.
 * *******************************************************************/
 void tcp_abort (TCP_T *tcp_c);

 /*!
 * Function name: tcp_options
 * \return ERR_OK.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param options : [in/out] controller option. See TCP_OPTIONS_T for detail.
 * \brief The application calls tcp_options() in order to configure a 
 * connection. One possible configuration is to delay the incoming
 * frames acknowledgment. If the application sets the ACK dalay then
 * the application should look at tcp_ack().
 * *******************************************************************/
err_t tcp_options(TCP_T *tcp_c, TCP_OPTIONS_T options);

 /*!
 * Function name: tcp_ack
 * \return ERR_OK.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \brief The TCP protocol is connection oriented. The protocol 
 * acknowledges each incoming frame.
 * cIPS acknowledges an incoming frame as soon as the application completes 
 * its processing. (FYI:in addition, if the application uses tcp_write() 
 * then cIPS multiplexes the acknowledgment with the outgoing frame). 
 * The consequence is that when the peer device receives the acknowledgment,
 * it can send its next frame. The result is very fast. The peer device receives 
 * the acknowledgment much sooner than the 200 milliseconds allowed for an ACK.
 * But in one situation, the fast speed can increase the traffic network.
 * The situation is: cIPS initiates a dialogue with the peer device
 * cIPS periodically sends data to the peer device, the period is less than 
 * 200 milliseconds and the peer device replies with some data.
 *
 * cIPS ------ PSH(frame(N))              -----> peer device
 *      <----- PSH(frame(M)),ACK(frame(N)) -----
 *      ------ ACK(frame(M))               ---->
 *  |
 *  | less than 200 ms but greater than 1 ms.
 *  |
 *  V
 *  cIPS ------ PSH(frame(N+1))           -----> peer device
 *       ...
 * cIPS can multiplex the two frames ACK(frame(M)) and PSH(frame(N+1)) 
 * into one frame ACK(frame(M)),PSH(frame(N+1)). To do so, cIPS delays 
 * the ACK(frame(M)). But as cIPS does not know that the application 
 * would send a frame after 1ms but before 200ms, the cIPS default 
 * behavior is to acknowledge immediately unless the application
 * configures the connection in order not to do it.
 * The application configures the delay by calling 
 * tcp_options(tcp_delay_ack_reply). Then tcp_write() would acknowledge 
 * the previous incoming frame or the application can force it by 
 * calling tcp_ack().
 * *******************************************************************/
err_t tcp_ack(TCP_T *tcp_c);

  /*!
 * Function name: tcp_delete
 * \return ERR_APP if the application calls tcp_delete() on the 
 * wrong condition (i.e the connection is not closed).
 * \param tcp_c: [in/out]connection of interest.
 * \brief tcp_delete() is the opposite of tcp_new(). tcp_new() allocates
 * the resource and configures it (IP address, port#...). tcp_delete()
 * frees the resource. It implies that the resource can be reused.
 * \note 1) There are three kinds of tcp connections: server, inherited
 * from a server, client.
 * 1.1) The application cannot delete a TCP server (see comment below).
 * 1.2) cIPS deletes the connection inherited from a server automatically
 * when the connection goes into a CLOSED state.
 * 1.3) The remaining kind is client. tcp_delete() exists for the client
 * connections. If the application wants to re-connect a remote server
 * with a different port number for example then it calls tcp_delete()
 * then tcp_new() with the new port number.
 * 2) tcp_delete() frees the resources ONLY if the connection is in 
 * a closed state. The connection can be closed because of a 
 * re-transmission timeout or because the application called tcp_close()
 * or tcp_abort(). The application can call tcp_delete() only after the 
 * these conditions.
 * *******************************************************************/
err_t tcp_delete(TCP_T *tcp_c);

/* TCP management */

/*!
 * Function name: tcp_init
 * \return nothing
 * \brief description: Check the boundary of TCP_MSS.
 * *******************************************************************/
void tcp_init (void);

/*!
 * Function name: tcp_timer
 * \return ERR_OK or ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER, ERR_RST
 * \param net_adapter : [in/out] adapter of interest.
 * \brief
 * Called every 500 ms and implements the retransmission and timeout
 * timer. It also increments various timers such as the inactivity 
 * timer in each controller.
 * *******************************************************************/
err_t tcp_timer (struct NETIF_S* net_adapter);

/* Lower layer interface to TCP: */

/*!
 * Function name: tcp_demultiplex
 * \return ERR_CHECKSUM or ERR_OK
 * \param ip_frame : [in] IP frame.
 * \param ip_frame_length : [in] Length of IP header + TCP header + app_data.
 * \param net_adapter : [in/out] network adapter.
 * \brief The IP layer (ip_parse()) identifies a TCP frame and forwards it to 
 * the TCP layer by calling tcp_demultiplex(). tcp_demultiplex() links 
 * the frame to its TCP controller. The TCP can be a TCP client, a
 * TCP server listening or might not exist.
 * *******************************************************************/
err_t tcp_demultiplex(u8_t* ip_frame, u32_t ip_frame_length, struct NETIF_S *net_adapter);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_H__ */

