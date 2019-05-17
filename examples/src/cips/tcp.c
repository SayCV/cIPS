#ident "@(#) $Id: tcp.c,v 1.6 2013/12/09 20:18:38 jdavid Exp $"
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
* \file tcp.c
* \brief TCP layer. The reference is the RFC793.
* The following code implement the "TCP Connection State Diagram"
* of the RFC793.
***************************************************/

#include <stdio.h> //for sprintf
#include "basic_c_types.h"
#include "nw_protocols.h"
#include "arp.h"
#include "ip.h"
#include "netif.h"
#include "err.h"
#include "debug.h"
#include "tcp.h"

#define TCP_MTU (NETWORK_MTU - (sizeof(TCP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T) + ETHER_CRC_LENGTH)) //Max data in a segment
#define ETH_IP_TCP_HEADER_SIZE ( sizeof(ETHER_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(TCP_HEADER_T) )

/*!Type of socket to indicate whether the socket can be reused when CLOSED
TCP_PERSISTENT: the socket can be closed and reopened (i.e it keeps its parameters (IP addresses, ports#...)).
TCP_NON_PERSISTENT: the socket is created by a server and dies when closed (i.e it can be reused with different parameters)*/
typedef enum{ TCP_PERSISTENT = 0, TCP_NON_PERSISTENT = 1 } TCP_CATEGORY;

static const u32_t DEFAULT_500ms_NB = ~0; //!<In ticks. Default value use to call tcp_c->tcp_check_connection(). Overriden by tcp_check_connection().

/*!TCP flags*/
#define TCP_FIN 0x01U
#define TCP_SYN 0x02U
#define TCP_RST 0x04U
#define TCP_PSH 0x08U
#define TCP_ACK 0x10U
#define TCP_URG 0x20U //Not supported

#define TCP_FLAGS_MASK 0x3FU
#define TCP_GET_HEADER_LENGTH(pheader) ((ntohs((pheader)->data_offset_flags) >> 12)*sizeof(u32_t)) //!<<in bytes
#define TCP_SET_HEADER_LENGTH(pheader, header_length_in_bytes) (pheader)->data_offset_flags = htons( (((header_length_in_bytes)/sizeof(u32_t)) << 12) | TCP_GET_FLAGS(pheader))
#define TCP_GET_FLAGS(pheader)  (ntohs((pheader)->data_offset_flags) & TCP_FLAGS_MASK)
#define TCP_SET_FLAGS(pheader, flags) (pheader)->data_offset_flags = htons((ntohs((pheader)->data_offset_flags) & ~TCP_FLAGS_MASK) | (flags & TCP_FLAGS_MASK))

static err_t tcp_process_application_events(TCP_T *tcp_c, const TCP_USER_COMMAND command, void* arg);
static err_t tcp_process_network_events(TCP_T *tcp_c, u16_t flags, TCP_HEADER_T *tcphdr, u32_t app_data_length);
static err_t tcp_process_timer_events(TCP_T *tcp_c);
static u32_t tcp_parse_options(const u8_t* const option);
static err_t tcp_store_error( const err_t err, TCP_T* const tcp_c, const s8_t* const function_name, const u32_t line_number);
static err_t tcp_send_control (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment, const u8_t control_bits, const u8_t* const options, const u8_t options_length);
static err_t tcp_build_data_ethernet_frame (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment, const u8_t* const pdata, const u32_t app_len,const u8_t control_bits);
static err_t tcp_build_control_ethernet_frame (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment, const u8_t control_bits, const u8_t* const options, const u8_t optlen);
static err_t tcp_recv_null(void *arg, TCP_T *tcp_c,  void* data, u32_t data_length);
static void tcp_need_acknowledgment (TCP_SENDING_SEG_T* segment, const u32_t app_AND_option_len, const u32_t seqno);
static void tcp_lookup_segment_by_acknowledge_no(TCP_T* const tcp_c, const u32_t ack_no);
static TCP_T * tcp_alloc(  struct NETIF_S* net_adapter, const u32_t type);
static void tcp_order_active_list( TCP_T** tcp_active_cs, const struct NETIF_S* const net_adapter);
static TCP_T* malloc_tcp_c( struct NETIF_S* net_adapter);
static err_t tcp_reset (TCP_T* const tcp_c,  TCP_SENDING_SEG_T* const segment, const s8_t* const func, const u32_t line);
static err_t tcp_create_child(TCP_T *tcp_c, u8_t* ip_frame);
static void tcp_remove_server_child_cs( struct NETIF_S* net_adapter );
static void segment_init_resource(TCP_T* tcp_c);
static void segment_init_connection (TCP_T* const tcp_c, const u8_t* const dest_mac_addr);
static void segment_change_state(TCP_T* tcp_c, TCP_SENDING_SEG_T *elt,  const seg_state new_state);
static TCP_SENDING_SEG_T * segment_get_first( TCP_T* tcp_c, const seg_state state);
static TCP_SENDING_SEG_T * segment_get_first_unused_after_unacked( TCP_T* tcp_c, const seg_state state);
static void tcp_register( TCP_T** tcp_active_cs,  TCP_T* tcp_c );
static void tcp_remove( TCP_T** tcp_active_cs,  TCP_T* tcp_c );
static u16_t tcp_new_port( struct NETIF_S* net_adapter);
static u8_t* tcp_memcpy(u8_t* output, const u8_t* input, const u32_t length);
static u32_t tcp_format_max_segment_size_option(const u16_t mss);


#if TCP_DEBUG
static void tcp_debug_print_flags(u8_t flags);
static void tcp_debug_print_state(enum tcp_state s);
static void tcp_debug_print_command(u32_t command);
#else
#  define tcp_debug_print_flags(flags)
#  define tcp_debug_print_state(s)
#  define tcp_debug_print_command(command)
#endif /* TCP_DEBUG */

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
err_t tcp_demultiplex(u8_t* ip_frame, u32_t ip_frame_length, struct NETIF_S *net_adapter)
{
  u32_t tcp_frame_length;
  TCP_HEADER_T* tcphdr;
  IP_HEADER_T* iphdr; 
  u32_t checksum;
  u32_t tcp_length;
  u32_t ip_header_length;
  err_t err = ERR_OK;
  u16_t pseudo_checksum;

  iphdr = (IP_HEADER_T*) ip_frame; 
  ip_header_length = IP_GET_HEADER_LENGTH(iphdr);
  tcphdr = (TCP_HEADER_T *)(ip_frame + ip_header_length);

  //Verify TCP checksum. The pseudo checksum takes into account the IP portion 
  //whereas the next checksum is on the TCP portion only.
  tcp_length = ip_frame_length - ip_header_length;
  pseudo_checksum = eth_build_pseudo_header( ntohl(iphdr->dest_addr) , ntohl(iphdr->source_addr), tcp_length - TCP_GET_HEADER_LENGTH(tcphdr), TCP_GET_HEADER_LENGTH(tcphdr), IP_TCP);
  if(pseudo_checksum != tcphdr->chksum)
  {
    checksum = ip_checksum( (const u16_t*)tcphdr, tcp_length );
    checksum += (u32_t)pseudo_checksum;
    //Fold 32-bit sum to 16 bits and add carry
    while( checksum >> 16) {
      checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }
  }
  else
  {
    //The peer has delegated the checksum to the adapter but the adapter has not calculated it (when it should had!).
    //As a result the frame only has its pseudo checksum. Wireshark displays "maybe caused by "TCP chcksum offload").
    checksum = CHECKSUM_OK;
  }

  if( checksum == CHECKSUM_OK)
  {

  //The peer device sends a TCP frame, cIPS looks in its list for the TCP controller matching
  //the incoming frame feature (port, ip address...)
  TCP_T* tcp_c = net_adapter->tcp_active_cs;
  while((tcp_c != NULL) && !((tcp_c->local_port == ntohs(tcphdr->dest_port))
  && (tcp_c->remote_port == ntohs(tcphdr->source_port)) && (tcp_c->remote_ip == ntohl(iphdr->source_addr)) ))
  {
    tcp_c = tcp_c->next;
  }

  if (tcp_c != NULL) // The incoming segment belongs to a connection.
  {
    if ( (ntohs(tcphdr->data_offset_flags) & TCP_RST) != TCP_RST)
    {
      tcp_frame_length = ip_frame_length - ( ip_header_length + TCP_GET_HEADER_LENGTH(tcphdr));
      err = tcp_process_network_events(tcp_c, ntohs(tcphdr->data_offset_flags), tcphdr, tcp_frame_length );
    }
    else
    {
      //1. cIPS notifies the application with an error code
      err = tcp_store_error( ERR_RST, tcp_c, __func__, __LINE__);
      //2. cIPS closes the tcp_c without sending a message to the peer device
      tcp_c->state = CLOSED;
      (void)tcp_remove(&(net_adapter->tcp_active_cs), tcp_c);
      if(tcp_c->closed) 
      {// cIPS notifies the application with a callback.
       //The callback gives the application the opportunity to do some processing of its choice.
        err = tcp_c->closed( tcp_c->callback_arg, tcp_c, err );
      }
    }
  }
  else
  {
    //Finally, if cIPS still did not get a match, the TCP frame might be a TCP client trying 
    //to connect a cIPS TCP server. So cIPS tries to match the peer device frame with a TCP server.
    //The TCP server are controller in the LISTENing state. 
    //So cIPS checks all TCP controllers that are LISTENing for incoming connections.
    TCP_T* ltcp_c = net_adapter->tcp_server_cs;
    while((ltcp_c != NULL) && !(ltcp_c->local_port == ntohs(tcphdr->dest_port)) )
    {
      ltcp_c = ltcp_c->next;
    }
    if( ltcp_c != NULL)
    {
      u16_t control_bits = TCP_GET_FLAGS(tcphdr) & TCP_FLAGS_MASK;

      if(control_bits == TCP_SYN)
      {
        T_DEBUGF(TCP_DEBUG, ("New TCP client on server port %d.\r\n",ltcp_c->local_port));
        err = tcp_process_application_events(ltcp_c, TCP_USER_SEND, (void*)ip_frame);
      }
      else if( (control_bits & TCP_RST) == TCP_RST)
      {
        if(ltcp_c->state != LISTEN){
          err = tcp_store_error( ERR_RST, ltcp_c, __func__, __LINE__);
        }else{
          err = ERR_OK;
        }
	  }
    }
  }
  }
  else
  {
    T_ERROR(("%s#%d:TCP: checksum error, frame dropped\r\n", net_adapter->name, ntohs(tcphdr->dest_port)));
    err = adapter_store_error( ERR_CHECKSUM, net_adapter, __func__, __LINE__);
  }
  return err;
}

/*!
 * Function name: tcp_process_network_events
 * \return ERR_OK, ERR_SEG_MEM, ERR_BUF, ERR_STREAMING
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param flags : [in] Control bits of the TCP frame.
 * \param tcphdr : [in] TCP header of the ethernet frame.
 * \param app_data_length : [in] Length of application data.
 * \brief Manage TCP messages coming from the network against the 
 * current state of the tcp_c.
 * *******************************************************************/
static err_t tcp_process_network_events(TCP_T *tcp_c, u16_t flags, TCP_HEADER_T *tcphdr,
 u32_t app_data_length)
{
  TCP_SENDING_SEG_T* first_segment;
  err_t err = ERR_OK;

#if TCP_DEBUG
  T_DEBUGF(TCP_DEBUG,("%s#%d: TCP ", tcp_c->netif->name, ntohs(tcphdr->dest_port)));
  tcp_debug_print_state(tcp_c->state);
  T_DEBUGF(TCP_DEBUG,(", flag=("));
  tcp_debug_print_flags(TCP_GET_FLAGS(tcphdr));
  T_DEBUGF(TCP_DEBUG,(")\r\n"));
#endif //TCP_DEBUG

  switch(tcp_c->state)
  {
    case  ESTABLISHED:
    //Once the TCP connection between the peer device and cIPS is open,
    //the cIPS TCP controller is in the ESTABLISHED state.
    if( (flags & TCP_ACK) == TCP_ACK)
    {
      //if the peer device multiplexes an ACK with some data (PSH), cIPS process the ACK then the data.
      //1. ACK: cIPS looks up the corresponding frame and acknowledge it.
      //It acknowledges by removing the ACK from the array of waiting ACK
      //2. PSH: 
      //Note: this "if" must be before "if( (flags & TCP_PSH) == TCP_PSH)"
      (void) tcp_lookup_segment_by_acknowledge_no( tcp_c, ntohl(tcphdr->ackno));
      //Update window size
      tcp_c->remote_wnd = ntohs(tcphdr->windowsize);
      if( ((flags & TCP_PSH) == 0) && (app_data_length != 0) ) //Means that the peer device sends cIPS a stream or a big file.
      {
        if( (tcp_c->stream_sequence != (MAX_TCP_SEG -1)) && (app_data_length <= tcp_c->local_mss))// -1 is the boundary because the last packet has a PSH flag in addtion to the ACK.
        {
          if(tcp_c->stream_sequence==0) {tcp_c->seqno_ori = ntohl(tcphdr->seqno);}
          //Store the incoming stream in a buffer
          tcp_memcpy(tcp_c->stream_position,(u8_t*)tcphdr + (u32_t)TCP_GET_HEADER_LENGTH(tcphdr),app_data_length);
          tcp_c->stream_position += app_data_length; //advance the pointer position
          (tcp_c->stream_sequence)++; //stream_sequence counts how many segment are used.
          if(tcp_c->seg_nb[TCP_SEG_UNSENT] ==0)
          { //cIPS receives a TCP frame and has one to send to the peer device. 
            //It multiplexes the ACK of the received frame with the data of the one it is sending to the peer device.
            tcp_c->remote_seqno = ntohl(tcphdr->seqno) + app_data_length; //Sequence number to acknowledge
            err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
          }
        }
        else
        {
          T_ERROR(("%s#%d: Incoming stream bigger than the tcp_c buffer\r\n", tcp_c->netif->name, ntohs(tcphdr->dest_port)));
          err =tcp_store_error( ERR_BUF, tcp_c, __func__, __LINE__);
          tcp_c->stream_position = tcp_c->incoming_stream;
          tcp_c->stream_sequence = 0;
        }
      }
      //If cIPS sends a large message to the peer device, cIPS holds the message in several segments.
      //When the ACK of the first segment arrives, cIPS sends the second segments and so on.
      //Note: if the message holds in one segment then cIPS sends it directly in tcp_write()
      //and does not use this piece of code.
      if(tcp_c->seg_nb[TCP_SEG_UNSENT])
      {
        //look up unsent segment.
        first_segment = segment_get_first( tcp_c, TCP_SEG_UNSENT);
        //Send next "unsent" segment.
        err = netif_send(tcp_c->netif,first_segment->frame, first_segment->len);
        //Move the segment from the "unsent" list to the "unacked" one.
        (void)segment_change_state( tcp_c, first_segment, TCP_SEG_UNACKED);
      }
    }
    if( (flags & TCP_PSH) == TCP_PSH) //if the peer device sends data to cIPS: cIPS processes them.
    {
      if( tcp_c->remote_seqno == ntohl(tcphdr->seqno)) //Detects the case when the peer device sends a request, the cIPS replies and the reply is lost. Then cIPS waits for the peer to retransmit but does not process the request a second time.
      {
      //Next_state : ESTABLISHED
      //to the application layer (if the app uses tcp_write() then tcp_write() will send the ACK).
      T_ASSERT(("%s#%d Register a callback with tcp_recv() on TCP controller 0x%lx\r\n",__func__, __LINE__, (u32_t)tcp_c), tcp_c->recv != NULL);
      tcp_c->remote_ACK_counter++;
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + app_data_length; //Sequence number to acknowledge
      if(tcp_c->stream_sequence == 0)//Not a stream: the peer device sent a frame smaller a TCP segment.
      {err = tcp_c->recv( tcp_c->callback_arg, tcp_c,(void*)((u8_t*)tcphdr + (u32_t)TCP_GET_HEADER_LENGTH(tcphdr)), app_data_length);}
      else
      { 
        //If the streaming reception went OK, we should have tcp_c->stream_sequence * TCP_MSS == ntohl(tcphdr->seqno) (Note: app_data_length is not yet included in tcphdr->seqno).
        //Otherwise, if frames are lost so tcp_c->stream_sequence is lower than expectected. As a result (tcp_c->stream_sequence * TCP_MSS) != ntohl(tcphdr->seqno) indicates lost of frames.
        //TO DO : reorder a stream.
        if( tcp_c->stream_sequence * tcp_c->local_mss == ntohl(tcphdr->seqno) - tcp_c->seqno_ori)
        {
          //CIPS completes the stream and sends it to the application.
          tcp_memcpy(tcp_c->stream_position,(u8_t*)tcphdr + (u32_t)TCP_GET_HEADER_LENGTH(tcphdr),app_data_length);
          //Note: if tcp_c->recv uses tcp_write then cIPS multiplexes the tcp_write with the received frame acknowlegment: the purpose of tcp_c->remote_ACK_counter is to signal a multiplexing situation to tcp_write.
          err = tcp_c->recv( tcp_c->callback_arg, tcp_c,(void*)tcp_c->incoming_stream, tcp_c->stream_sequence * tcp_c->local_mss + app_data_length);
        }
        else
        {
           T_ERROR(("%s#%d: Streaming failed because of frame(s) lost\r\n",tcp_c->netif->name, ntohs(tcphdr->dest_port)));
           err =tcp_store_error( ERR_STREAMING, tcp_c, __func__, __LINE__);
        }
        tcp_c->stream_position = tcp_c->incoming_stream;
        tcp_c->stream_sequence = 0;
      }
      //Send ACK. (multiplex with the possibly tcp_write()).
      if(tcp_c->seg_nb[TCP_SEG_UNSENT])
      {
        //look up unsent segment.
        first_segment = segment_get_first( tcp_c, TCP_SEG_UNSENT);
        //Send next "unsent" segment.
        err = netif_send(tcp_c->netif,first_segment->frame, first_segment->len);
        //Move the segment from the "unsent" list to the "unacked" one.
        (void)segment_change_state( tcp_c, first_segment, TCP_SEG_UNACKED);
       }
      else
      {
        if( tcp_c->remote_ACK_counter){ //cIPS must reply with a ACK.
          if( (!(tcp_c->options & tcp_delay_ack_reply))){ //if there has been no tcp_write() to send the ACK then send it.
            err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
            tcp_c->remote_ACK_counter = 0;
          } else { //cIPS must reply with a ACK but the option tells to delay it.
            tcp_c->remote_ACK_counter++; //delay the ACK. As a result tcp_write() or tcp_ack() must send the ACK.
          }
        }
      }
      } //end of if( tcp_c->remote_seqno != ntohl(tcphdr->seqno))
    }
    if( (flags & TCP_FIN) == TCP_FIN)
    {
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + 1; //Sequence number to acknowledge
      // 1. rcv FIN
      // 2. snd FIN|ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_FIN|TCP_ACK, NULL, 0);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
      //3.Next_state : LAST_ACK (skip CLOSE_WAIT because other stacks expect FIN and ACK is the same frame)
      tcp_c->state = LAST_ACK;
    }
    tcp_c->counter_of_500ms = 0; //Reset counter to mean that activity is going on.
    break;
  case CLOSED: //represents no connection state at all.
    break;
  case LISTEN: //Do nothing. CIPS detects a client in tcp_demultiplex().
    break;
  case SYN_SENT: //represents waiting for a matching connection request after having sent a connection request.
    if( (flags & (TCP_SYN | TCP_ACK)) == ( TCP_SYN | TCP_ACK))
    {
      //1. rcv SYN|ACK
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + 1 ;
      tcp_c->remote_wnd = ntohs(tcphdr->windowsize);
      //2. snd ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
      //3.Remove the ACK from the array of waiting ACK
      (void) tcp_lookup_segment_by_acknowledge_no( tcp_c, ntohl(tcphdr->ackno));
      //4. Next_state : ESTABLISHED
      tcp_c->state = ESTABLISHED;
    }
    else if( (flags & TCP_SYN) == TCP_SYN)
    {
      //1. rcv SYN
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + 1 ;
      tcp_c->remote_wnd = ntohs(tcphdr->windowsize);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
      //2. snd ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
      //3.Next_state : SYN_RCVD
      tcp_c->state = SYN_RCVD;
    }
    break;
  case SYN_RCVD: //represents waiting for a confirming connection request acknowledgment after having both received and sent a connection request.
    if( (flags & TCP_ACK) == TCP_ACK)
    {
      //Next_state : ESTABLISHED
      tcp_c->state = ESTABLISHED;
      //1. rcv ACK of SYN
    }
    break;
  case FIN_WAIT_1: //represents waiting for a connection termination request from the remote TCP, or an acknowledgment of the connection termination request previously sent.
    if( (flags & TCP_ACK) == TCP_ACK)
    {
      //Next_state : FIN_WAIT_2
      tcp_c->state = FIN_WAIT_2;
      // 1. rcv ACK of FIN
    }
    if( (flags & TCP_FIN) == TCP_FIN)
    {
      //Next_state : CLOSING
      tcp_c->state = CLOSING;
      // 1. rcv FIN
      // 2. snd ACK
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + 1 ;
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
    }
    break;
 case FIN_WAIT_2: //represents waiting for a connection termination request from the remote TCP.
    if( (flags & TCP_FIN) == TCP_FIN)
    {
      //Next_state : TIME_WAIT -> CLOSED
      tcp_c->state = TIME_WAIT;
      // 1. rcv FIN
      // 2. snd ACK
      tcp_c->remote_seqno = ntohl(tcphdr->seqno) + 1 ;
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
    }
    break;
  case CLOSE_WAIT: //represents waiting for a connection termination request from the local user.
    {
      //Next_state : LAST_ACK
      tcp_c->state = LAST_ACK;
      // 1. rcv FIN
      // 2. snd ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_FIN, NULL, 0);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
    }
    break;
  case CLOSING: //represents waiting for a connection termination request acknowledgment from the remote TCP.
    if( (flags & TCP_ACK) == TCP_ACK)
    {
      //Next_state : TIME_WAIT -> CLOSED
      tcp_c->state = TIME_WAIT;
      // 1. rcv ACK of FIN
    }
    break;
  case LAST_ACK: //represents waiting for an acknowledgment of the connection termination request previously sent to the remote TCP (which includes an acknowledgment of its connection termination request).
    if( (flags & TCP_ACK) == TCP_ACK)
    {
      //Next_state : CLOSED
      tcp_c->state = CLOSED;
      (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
      if(tcp_c->closed) //Report to the application.
      { err = tcp_c->closed( tcp_c->callback_arg, tcp_c, err );}
      // 1. rcv ACK of FIN
    }
    break;
  default:
    break;
  }
  return err;
}

/*!
 * Function name: tcp_process_timer_events
 * \return ERR_OK, ERR_RST
 * \param tcp_c : [in/out] tcp_c of interest.
 * \brief Check if this TCP controller has stayed too long in some "dead" states.
 * For example, the peer device negociates a close connection with cIPS.
 * In the middle of the negociation, the ethernet cable is disconnected.
 * CIPS detects that that kind of situation by checking whether a TCP 
 * controller have been waiting for messages for too much time.
 * *******************************************************************/
static err_t tcp_process_timer_events(TCP_T *tcp_c)
{
  err_t err = ERR_OK;
  //1. Check if this TCP controller has stayed too long in some "dead" states.
  switch(tcp_c->state)
  {
    case ESTABLISHED: //state hanled by tcp_check_connection()
    break;
    case LISTEN:
    case SYN_SENT:
      tcp_c->counter_of_500ms = 0; //Reset counter to mean that activity is going on.
    break;
    case FIN_WAIT_1:
    case CLOSING:
    case FIN_WAIT_2:
      if ( ++(tcp_c->timer) == (u32_t)(TCP_FIN_WAIT_TIMEOUT / TCP_TIMER_PERIOD))
      {
        tcp_c->state = TIME_WAIT;
        T_DEBUGF(TCP_DEBUG,("%s#%d: timeout in FIN-WAIT-x\r\n", tcp_c->netif->name, tcp_c->local_port));
      }
    break;
    case LAST_ACK: //In case "ACK of FIN" is not received (note: Labview does not send "ACK of FIN")
    case TIME_WAIT:
        tcp_c->state = CLOSED;
        (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
        if(tcp_c->closed) //Report to the application.
        {err = tcp_c->closed( tcp_c->callback_arg, tcp_c, ERR_OK );}
        T_DEBUGF(TCP_DEBUG,("%s#%d: timeout in TIME_WAIT: tcp_c removed\r\n", tcp_c->netif->name, tcp_c->local_port));
    break;
    case SYN_RCVD:
      if ( ++(tcp_c->timer) == (u32_t)(TCP_SYN_RCVD_TIMEOUT / TCP_TIMER_PERIOD))
      {
        err = tcp_reset (tcp_c, &tcp_c->control_segment, __func__, __LINE__);
        tcp_c->state = CLOSED;
        (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
        T_DEBUGF(TCP_DEBUG,("%s#%d: timeout in SYN_RCVD: tcp_c removed\r\n", tcp_c->netif->name, tcp_c->local_port));
      }
    break;
    default:
    break;
  }
  return err;
}

/*!
 * Function name: tcp_process_application_events
 * \return ERR_TCP_MEM or ERR_OK
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param command : [in] Can be: TCP_USER_NO_COMMAND,
 * TCP_USER_PASSIVE_OPEN,TCP_USER_ACTIVE_OPEN, TCP_USER_SEND, TCP_USER_CLOSE.
 * \param arg : [in] potential data.
 * \brief Manage TCP messages coming from the application against the 
 * current state of the tcp_c.
 * *******************************************************************/
static err_t tcp_process_application_events(TCP_T *tcp_c, const TCP_USER_COMMAND command, void* arg)
{
  err_t err = ERR_OK;

#if TCP_DEBUG
  T_DEBUGF(TCP_DEBUG,("%s#%d: TCP ", tcp_c->netif->name, tcp_c->local_port));
  tcp_debug_print_state(tcp_c->state);
  T_DEBUGF(TCP_DEBUG,(", command=("));
  tcp_debug_print_command(command);
  T_DEBUGF(TCP_DEBUG,(")\r\n"));
#endif //TCP_DEBUG

  switch(tcp_c->state)
  {
    case ESTABLISHED: //a connection is open between the peer device and cIPS.
    if( command == TCP_USER_CLOSE)
    {
      // 1. snd FIN (The RFC expects an ACK but Labview does not ACK so do not expect an ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_FIN, NULL, 0);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
      //2. Next_state : FIN_WAIT_1
      tcp_c->state = FIN_WAIT_1;
    }
    break;
    case CLOSED: //CLOSED also means that it is available and that cIPS can re-use it.
    if( command == TCP_USER_PASSIVE_OPEN)
    { //The application opens a TCP server.
      //1.Remove from the list of Clients
      tcp_c->state = CLOSED; //Condition to be removed.
      (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
      //2. Register to the list of Servers.
      tcp_c->state = LISTEN;
      (void)tcp_register(&(tcp_c->netif->tcp_server_cs), tcp_c);
    }
    if( command == TCP_USER_ACTIVE_OPEN)
    { //The application opens a TCP client to a remote server.
      TCP_SENDING_SEG_T* first_segment;
      u32_t options;
      //Build an MSS option.
      options = tcp_format_max_segment_size_option(tcp_c->local_mss);

      (void)segment_init_resource(tcp_c); //If CIPS reuses a controller then empty the potential remaining frames in the segments.
      //Note: cIPS initialized the segements in tcp_connect().
      first_segment = segment_get_first( tcp_c, TCP_SEG_UNUSED);
      err = tcp_send_control (tcp_c, first_segment, TCP_SYN, (u8_t *)&options, sizeof(options));
      if( !err )
      {
        tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
        (void)tcp_need_acknowledgment (first_segment, sizeof(options), tcp_c->local_seqno);
        //The application creates a TCP connection, cIPS sends the first signal TCP_SYN and 
        //keeps it in case cIPS needs to retransmit it. It keeps it by moving the segment 
        //from the "unused" list to the "unacked" one.
        (void)segment_change_state( tcp_c, first_segment, TCP_SEG_UNACKED);
        // 1. Next_state : SYN_SENT
        tcp_c->state = SYN_SENT;
        (void)tcp_register(&(tcp_c->netif->tcp_active_cs), tcp_c);
      }
    }
    break;
  case LISTEN: //CIPS has a TCP server.
    if( command == TCP_USER_SEND) // The peer device is a client and requests a new connection to the server.
    {
      err = tcp_create_child(tcp_c , (u8_t*)arg);
    }
    if( command == TCP_USER_CLOSE) // The application decides to close its TCP server
    {
      // 1. Next_state : CLOSED
      tcp_c->state = CLOSED;
      // 2. delete TCB
      (void)tcp_remove(&(tcp_c->netif->tcp_server_cs), tcp_c);
    }
    break;
  case SYN_SENT: //The application is opening a connection to a remote server but decides to close it.
    if( command == TCP_USER_CLOSE)
    {
      // 1. Next_state : CLOSED
      tcp_c->state = CLOSED;
      // 2. delete TCB
      (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
      if(tcp_c->closed) //Report to the application.
      { err = tcp_c->closed( tcp_c->callback_arg, tcp_c, err );}
    }
    break;
  case SYN_RCVD:
    if( command == TCP_USER_CLOSE) //The application server is opening a connection with a peer device client but decides to close it.
    {
      // 1. snd FIN (The RFC expect an ACK but Labview does not ACK so do not expect an ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_FIN, NULL, 0);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
      //2. Next_state : FIN_WAIT_1
      tcp_c->state = FIN_WAIT_1;
    }
    break;
  case CLOSE_WAIT:
    if( command == TCP_USER_CLOSE) //The CLOSE_WAIT state expects the TCP_USER_CLOSE message (see RFC793).
    {
      // 1. snd FIN (The RFC expect an ACK but Labview does not ACK so do not expect an ACK
      err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_FIN, NULL, 0);
      tcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
      // 2. Next_state : LAST_ACK -> CLOSED
      tcp_c->state = LAST_ACK;
    }
    break;
  case CLOSING:
    if( command == TCP_USER_CLOSE)
    {
      // 1. Next_state : CLOSED
      tcp_c->state = CLOSED;
      // 2. delete TCB
      (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
      if(tcp_c->closed) //Report to the application.
      { err = tcp_c->closed( tcp_c->callback_arg, tcp_c, err );}
    }
    break;
  case FIN_WAIT_1: //represents waiting for a connection termination request from the remote TCP, or an acknowledgment of the connection termination request previously sent.
  case FIN_WAIT_2: //represents waiting for a connection termination request from the remote TCP.
  case LAST_ACK: //represents waiting for an acknowledgment of the connection termination request previously sent to the remote TCP (which includes an acknowledgment of its connection termination request).
  default:
      T_ASSERT(("%s#%d TCP command #%d does not exist for the state #%d!\r\n",__func__, __LINE__, command, tcp_c->state),0);
    break;
  }

  return err;
}


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
err_t tcp_write(TCP_T *tcp_c, const void *app_data, u32_t app_len)
{
  err_t err = ERR_OK;

  T_ASSERT(("%s#%d TCP socket is not created \n", __func__, __LINE__), tcp_c != NULL);

  if( tcp_c->state == ESTABLISHED ) {
     //The application sends a message to a peer device. The peer device might not 
     //have enough memory to receive the message.
    if(((u32_t)tcp_c->remote_wnd) >= tcp_c->remote_mss){
      u32_t intermediate_length;
      u32_t segment_nb;
      //1. The application sends a message that can take several TCP segments. CIPS calculates the amount of segments it needs.
      segment_nb = (tcp_c->remote_mss)? app_len / tcp_c->remote_mss:0;
      if( (segment_nb * tcp_c->remote_mss) < app_len ) { segment_nb++;}//round up

      //2. Once cIPS has calculated the amount of segments, it checks whether it has these segments available.
      if( segment_nb > tcp_c->seg_nb[TCP_SEG_UNUSED] ){
        //Not enough space to store the data in multiple frames. If it was predictable, increase MAX_TCP_SEG.
        T_ERROR(("ERR_SEG_MEM : not enough space to hold the message. tcp_write(%ld bytes). Increase MAX_TCP_SEG\r\n", app_len));
        err =tcp_store_error( ERR_SEG_MEM, tcp_c, __func__, __LINE__);
      }

      intermediate_length = (app_len < tcp_c->remote_mss)? app_len: tcp_c->remote_mss;

      T_DEBUGF(TCP_DEBUG, ("%s#%d: %s %ld segment(s) for a total length of %ld bytes.\r\n",tcp_c->netif->name,tcp_c->local_port, __func__,segment_nb,app_len));

      //3. enqueue data into segments
      if( (!err) && (intermediate_length!=0)) {
        u8_t control_bits ;
        TCP_SENDING_SEG_T* unused_seg;
        u32_t i;
        //3.1 Fill in each segment and put it in the unsent queue.
        for( i = 0; i < segment_nb; i++) {
          if(!tcp_c->seg_nb[TCP_SEG_UNACKED]){
            unused_seg = segment_get_first( tcp_c, TCP_SEG_UNUSED);
          }else{
            unused_seg = segment_get_first_unused_after_unacked( tcp_c, TCP_SEG_UNUSED);
            if(!unused_seg){
              //cIPS claimed that there were enough UNUSED segments. Unfortunatly
              //there are enough but not after the last UNACKED so it put it where it can.
              //Consequently, cIPS cannot maintain the order of the segment. It will pass only if there is no retransmission.
              unused_seg = segment_get_first( tcp_c, TCP_SEG_UNUSED);
            }
          }
          control_bits = (i == segment_nb-1)?TCP_PSH:0; //Set TCP_PSH on the last segment.
          control_bits |= TCP_ACK; //A stream returns an ACK for each sub-segment.
          err = tcp_build_data_ethernet_frame (tcp_c, unused_seg, (u8_t*)app_data + i*tcp_c->remote_mss, intermediate_length, control_bits);
          tcp_c->local_seqno += intermediate_length;

          (void)tcp_need_acknowledgment (unused_seg, intermediate_length, tcp_c->local_seqno);

          if( i == 0 ) {//Send the frame directly
            err = netif_send(tcp_c->netif,unused_seg->frame, ETH_IP_TCP_HEADER_SIZE + intermediate_length);
            (void)segment_change_state( tcp_c, unused_seg, TCP_SEG_UNACKED);
            tcp_c->remote_ACK_counter = 0; //The app uses tcp_write. Tcp_write multiplexes PUSH and ACK. As tcp_write sends an ACK, cIPS does not need to send an individual ACK frame.
          } else {
            (void)segment_change_state( tcp_c, unused_seg, TCP_SEG_UNSENT);
          }

          unused_seg->retransmission_timer_slice = 0; //Reset retransmission timer for this segment.
          //next "intermediate_length".
          intermediate_length = ((i+1) != segment_nb-1)?tcp_c->remote_mss:(app_len - (segment_nb-1)*tcp_c->remote_mss);
        }
      }
    } else { //The window of the peer device is too small.
      err =tcp_store_error( ERR_PEER_WINDOW, tcp_c, __func__, __LINE__);
    }
  } else{ //The application uses tcp_write() when it is not connected
    T_DEBUGF(TCP_DEBUG, ("%s#%d: %s:The application uses tcp_write() when it is not connected\r\n",tcp_c->netif->name,tcp_c->local_port, __func__));
    err = ERR_APP;
  }

  return err;
}

/*!
 * Function name: tcp_init
 * \return nothing
 * \brief Check the boundary of TCP_MSS.
 * *******************************************************************/
void tcp_init(void)
{
  T_ASSERT(("%s#%d TCP_MSS(%d) must be lower than %d\n",__func__, __LINE__, TCP_MSS, TCP_MTU), TCP_MSS <= TCP_MTU);
  return;
}

/*!
 * Function name: tcp_close
 * \return ERR_OK.
 * \param tcp_c: [in/out]connection of interest.
 * \brief Send a close message.
 * *******************************************************************/
err_t tcp_close(TCP_T *tcp_c)
{
  return tcp_process_application_events(tcp_c, TCP_USER_CLOSE, NULL);
}

 /*!
 * Function name: tcp_abort
 * \return nothing.
 * \param tcp_c: [in/out]connection of interest.
 * \brief Send a RST and close the tcp_c.
 * *******************************************************************/
void tcp_abort(TCP_T *tcp_c)
{
  //1.Build the ethernet frame
  if(!tcp_build_control_ethernet_frame (tcp_c, &tcp_c->control_segment, TCP_RST, NULL, 0))
  {
    //2.Send the frame to the network
    (void)netif_send(tcp_c->netif,tcp_c->control_segment.frame, ETH_IP_TCP_HEADER_SIZE);

    //3. Close tcp_c
    tcp_c->state = CLOSED;
    (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
    (void)tcp_remove(&(tcp_c->netif->tcp_server_cs), tcp_c);
  }
  return;
}

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
err_t tcp_delete(TCP_T *tcp_c)
{
  err_t err = ERR_OK;
  //Note: a TCP server is never in a CLOSED state because always in a LISTEN state.
  //So the application cannot delete a server. I do not see any reason to delete
  //a server but if you need to delete it then feel free to adapt the code 
  //"if((tcp_c->state == CLOSED)||(tcp_c->state == LISTEN))"
  if(tcp_c->state == CLOSED){
    tcp_c->type = TCP_NON_PERSISTENT;
    tcp_c->id = UNUSED;
    (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
    (void)tcp_remove(&(tcp_c->netif->tcp_server_cs), tcp_c);
  } else{
    err = ERR_APP;
  }
  return err;
}

/*!
 * Function name: tcp_listen
 * \return the tcp_c in argument.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \brief The application creates a TCP server.
 * *******************************************************************/
err_t tcp_listen(TCP_T *tcp_c)
{
  return tcp_process_application_events(tcp_c, TCP_USER_PASSIVE_OPEN, NULL);
}


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
err_t tcp_connect(TCP_T *tcp_c, u32_t ipaddr, u16_t port, err_t (* connect)(void *arg, TCP_T *tcp_c))
{
  err_t err = ERR_OK;

  if( tcp_c->state == CLOSED ){
    u8_t dest_mac_addr[MAC_ADDRESS_LENGTH];
    u32_t dest_ip_or_gateway;
    T_ASSERT(("%s#%d Invalid IP@.\r\n", __func__, __LINE__), ipaddr != 0);

    dest_ip_or_gateway = arp_get_route(ipaddr, tcp_c->netif->gateway_addr, tcp_c->netif->netmask);

    //Check that the host is reachable.
    if (arp_query_cache(&(tcp_c->netif->arp_cache), dest_ip_or_gateway, dest_mac_addr, tcp_c->netif->netmask) != ERR_VAL){
      tcp_c->remote_ip = ipaddr;
      tcp_c->remote_port = port;
      if (tcp_c->local_port == 0) { tcp_c->local_port = tcp_new_port(tcp_c->netif);}

      tcp_c->connect = connect;
      (void)segment_init_connection (tcp_c, dest_mac_addr); //Initialize fields that are staying constant for the life of the connection
      //If CIPS reuses a controller (i.e. the application does not call tcp_new() 
      //a second time but re-connects the host with the same TCP controller)
      //then cIPS empties the potential remaining frames in the segments.
      (void)segment_init_resource(tcp_c);

      T_DEBUGF(TCP_DEBUG, ("%s#%d: tcp_connect to port %d\r\n", tcp_c->netif->name, tcp_c->local_port, port));
      err = tcp_process_application_events(tcp_c, TCP_USER_ACTIVE_OPEN, NULL);
    } else { //The destination MAC address is unknown, cIPS sends an ARP request to resolve it.
      u32_t framelen;
      framelen =  eth_build_frame( dest_mac_addr, tcp_c->netif->mac_address, dest_ip_or_gateway, tcp_c->local_ip, tcp_c->control_segment.frame, ETH_ARP_REQUEST);
      err = netif_send(tcp_c->netif,tcp_c->control_segment.frame, framelen);
      if(!err){
        err = tcp_store_error( ERR_MAC_ADDR_UNKNOWN, tcp_c, __func__, __LINE__);
      }
    }
  }

  return err;
}

/*!
 * Function name: tcp_timer
 * \return ERR_OK, ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER, ERR_RST.
 * \param net_adapter : [in/out] adapter of interest.
 * \brief 
 * Called every 500 ms and implements the retransmission and timeout
 * timer. It also increments various timers such as the inactivity 
 * timer in each controller.
 * \note The timer could be called every 500ms. The important point is that it must be lower than TCP_RETRANSMISSION_TIMEOUT.
 * 1. Steps through all of the active TCP controllers.
 * 1.1 check the unack list per tcp_c.
 * 1.2 re-send or reset the connection.
 * 2. Check if this TCP controller has stayed too long in some "dead" states.
 * 3. Check if the application should check the connection.
 * *******************************************************************/
err_t tcp_timer(struct NETIF_S* net_adapter)
{
  TCP_SENDING_SEG_T* unacked_seg;
  TCP_T *tcp_c;
  err_t err = ERR_OK;

  //1. Steps through all of the active TCP controllers.
  tcp_c = net_adapter->tcp_active_cs;
  while (tcp_c != NULL) //tcp_c_list
  {
    //When cIPS sends a TCP frame to the peer device , the peer device  must acknowledge it.
    //If the peer device does not acknowledge the frame fast enough, it means 
    //that the frame is lost. Then cIPS retransmits. If the peer device does 
    //not acknowledge after a few retransmissions then the peer device connection 
    //is down and cIPS confirms it by sending a reset.

    //1.1 check the unack list per tcp_c
    //Retrieve the first UNACKED segment.
    if (tcp_c->seg_nb[TCP_SEG_UNACKED]) //If there is a frame to retransmit.
    { //retransmit only one at a time
      unacked_seg = segment_get_first( tcp_c, TCP_SEG_UNACKED);
      if( unacked_seg->retransmission_timer_slice == 0 )
      { ++(unacked_seg->retransmission_timer_slice);} //skip the potential action. this is to prevent the case when tcp_timer() is called just after sending the msg. We have to wait TCP_TIMER_PERIOD s at least.
      else
      {
        ++(unacked_seg->retransmission_timer_slice);
        if( unacked_seg->retransmission_timer_slice >= tcp_c->retransmission_time_out)
        {
          //Send a TCP_RST and Close the TCP controller.
          err = tcp_reset(tcp_c, &tcp_c->control_segment, __func__, __LINE__);
        }
        else  //CIPS retransmits the unackwoledged frame or transmits the potential next segment.
        {
          if (tcp_c->seg_nb[TCP_SEG_UNSENT]==0)
          { //There is only one segment. In that condition, the peer device must send an ACK.
            //It did not so cIPS retransmits.
            err = netif_send(tcp_c->netif, unacked_seg->frame, unacked_seg->len);
          }else{
            if(unacked_seg->retransmission_timer_slice >= tcp_c->retransmission_time_out-1)
            { //There are several segments but the peer device has not acknowleged for quite 
              //a long time (tcp_c->retransmission_time_out-1) so cIPS retransmits.
              err = netif_send(tcp_c->netif, unacked_seg->frame, unacked_seg->len);
            }else{
              //When the application sends data larger than one segment (tcp_write()), cIPS splits
              //the data into several segments (or frames). It sends the first frame and waits for 
              //the ACK in order to send the next frame segment and so on. However, TCP does not 
              //necessary acknowledge every frame by an ACK. For example, HTTP on TCP does not 
              //acknowledge two consecutive frames with two ACKs but with only one ACK every two frames.
              //If cIPS sends one frame and the peer device acknowledges after two frames only and cIPS waits 
              //for an ACK, then the situation is stuck. tcp_timer() solves the situation.
              //If cIPS has some unacknowledged frames and unsent frames then it sends the next unsent
              //frame and expects the peer device to acknowledge. If the peer does not acknowledge for a while
              //then cIPS retransmits.
              TCP_SENDING_SEG_T* unsent_seg;
              unsent_seg = segment_get_first( tcp_c, TCP_SEG_UNSENT);
              err = netif_send(tcp_c->netif, unsent_seg->frame, unsent_seg->len);
              //Move the segment from the "unsent" list to the "unacked" one.
              (void)segment_change_state( tcp_c, unsent_seg, TCP_SEG_UNACKED);
            }
          }
        }
      }
    }

    //If a TCP connection connects a peer device to cIPS and if there is no traffic between the two.
    //Either the connection is broken or it is the normal state of operation between the peer device and the application not to exchange messages.
    //cIPS cannot make the difference between the two situations but the application can.
    //So cIPS counts the inactivity time and notifies the application (through the periodic_connection_check() callback.
    //The application can decide to close the connection, to test the connection or to do nothing.
    if(tcp_c->state != LISTEN){ //The LISTEN state is excluded because it represents a TCP server. TCP server waits for clients to connect. Once a client is connected or no other client connects, there is no traffic but it is normal.
      if(tcp_c->state != CLOSED){
        //2. Check if this TCP controller has stayed too long in a "no traffic" state.
         err = tcp_process_timer_events( tcp_c );

        //3. Check if we should check the connection.
        ++tcp_c->counter_of_500ms;
        if (tcp_c->counter_of_500ms == tcp_c->nb_of_500ms)
        {
          if(tcp_c->periodic_connection_check) //The application checks the connection.
          {err = tcp_c->periodic_connection_check( tcp_c->callback_arg, tcp_c );}
//NOTE: The library does not implement the TCP KEEP ALIVE property. But, if you need it,
//send a tcp message with no data in the "tcp_c->tcp_check_connection" callback.
//example: {err = tcp_write(tcp_c, NULL, 0);} //!< send an empty message.
          tcp_c->counter_of_500ms = 0;
        }
      } else if((tcp_c->connect) && (!err)){
        //The controller is a TCP client. It is in the CLOSED state but the application has called tcp_connect()
        //in order to reach a peer server. Reaching the peer server necessitates two steps:
        //1. to resolve the (IP,MAC) link. This step can take several ARP calls. It can last for ever if the server is not there.
        //2. to exchange the TCP connect message.
        //Potentially several times #1 and then #2 means that tcp_connect() might be call several times.
        //If one call is not enough, the purpose of the callback is to call again tcp_connect().
        //It is a callback rather than an internal call so that the application can decide what to do if
        //the server does not exist (keep trying or time-out). In addition, in the callback, the application
        //can space out the tcp_connect() to more than the tcp_timer() period.
        //Note 1: if the server disconnect the client, then the client goes to CLOSED state.
        //If the tcp_c->connect callback is defined then thc_timer() calls it and the client re-connects.
        //Note 2: the content of the callback can be:
        //err = tcp_connect(tcp_c, tcp_c->remote_ip, tcp_c->remote_port, tcp_c->connect);
        err = tcp_c->connect(tcp_c->callback_arg, tcp_c);
      }
    }

  tcp_c = tcp_c->next;
  }
  return err;
}

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
err_t tcp_options(TCP_T *tcp_c, TCP_OPTIONS_T options)
{
  err_t err = ERR_OK;
  if( tcp_c && (options & tcp_delay_ack_reply)){
    tcp_c->options = options;
  }
  return err;
}

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
err_t tcp_ack(TCP_T *tcp_c)
{
  err_t err = ERR_OK;

  if( tcp_c && (tcp_c->remote_ACK_counter) && (tcp_c->options & tcp_delay_ack_reply)){
    err = tcp_send_control (tcp_c, &tcp_c->control_segment, TCP_ACK, NULL, 0);
    tcp_c->remote_ACK_counter = 0;
  }

  return err;
}
/*!
 * Function name: tcp_recv_null
 * \return ERR_OK.
 * \param arg : [in] dummy param.
 * \param tcp_c : [in] dummy param.
 * \param data : [in] dummy param.
 * \param data_length : [in] dummy param.
 * \brief Default received callback for tcp_alloc().
 * *******************************************************************/
static err_t tcp_recv_null(void *arg, TCP_T *tcp_c,  void* data, u32_t data_length)
{
   T_ASSERT(("%s#%d Register a callback with tcp_accept() on TCP controller 0x%lx\n", __func__, __LINE__, (u32_t)tcp_c), tcp_c->accept != NULL);
  return ERR_OK;
}


/*!
 * Function name: tcp_alloc
 * \return the tcp_c created.
 * \param net_adapter : [in] Adapter of onterest.
 * \param type : [in] TCP_CLIENT or TCP_SERVER_CHILD.
 * \brief Create a new tcp_c.
 * *******************************************************************/
static TCP_T* tcp_alloc( struct NETIF_S* net_adapter, const u32_t type)
{
  TCP_T *tcp_c;

  tcp_c = malloc_tcp_c(net_adapter);
  if (tcp_c != NULL)
  {
    tcp_c->local_ip = 0;
    tcp_c->remote_ip = 0;
    tcp_c->netif = net_adapter;
    tcp_c->remote_ACK_counter = 0;
    tcp_c->next = NULL; //!< will be updated by tcp_register().
    tcp_c->state = CLOSED;
    tcp_c->callback_arg = NULL;
    tcp_c->local_port = 0;
    tcp_c->remote_port = 0;
    tcp_c->remote_seqno = 0;
    tcp_c->remote_wnd = TCP_MTU;
    tcp_c->remote_mss = TCP_MTU;
    tcp_c->timer = 0;
    tcp_c->counter_of_500ms = 0;
    tcp_c->nb_of_500ms = DEFAULT_500ms_NB;
    tcp_c->local_mss = (TCP_MSS < TCP_MTU)?TCP_MSS:TCP_MTU;
    tcp_c->local_wnd = (TCP_WND < TCP_MTU)?TCP_WND:TCP_MTU; //Sould be (TCP_MTU+4) because WND starts at the ACK long.
    tcp_c->retransmission_time_out = TCP_RETRANSMISSION_TIMEOUT / TCP_TIMER_PERIOD; //The retransmission time out is a counter representing time. It is the maximum amount of fraction of TCP_TIMER_PERIOD to reach TCP_RETRANSMISSION_TIMEOUT.
    tcp_c->local_seqno = ((u32_t)tcp_c) & 0xFF; //(added by JMD) ISS: ramdom number between 0 an 0xFF.
    (void)segment_init_resource(tcp_c);
    tcp_c->stream_position = tcp_c->incoming_stream;
    tcp_c->stream_sequence = 0;
    tcp_c->seqno_ori = 0;
    tcp_c->options = tcp_no_options;
    tcp_c->recv = tcp_recv_null;  // Function to be called when a connection has been set up.
    tcp_c->connect = NULL;
    tcp_c->accept = NULL;
    tcp_c->periodic_connection_check = NULL;
    tcp_c->closed = NULL;
    tcp_c->type = type;
  }
  return tcp_c;
}

/*!
 * Function name: tcp_new
 * \return tcp_c: [out] the tcp_c created or NULL if no more controllers can be created.
 * \param ipaddr : [in] remote IP address to reach.
 * \param port : [in] remote port to reach.
 * \param err : [out] ERR_TCP_MEM if more than MAX_TCP tcp connection already exist, ERR_VAL if the IP@ is wrong, ERR_OK otherwise.
 * \brief Create a new TCP Protocol Control Block or TCB (see RFC793).
 * \note if the application creates a TCP client then it declares that
 * it is a client by calling tcp_connect() and it specifies the IP address 
 * and port number of the remote server in the tcp_connect() arguments.
 * if the application creates a TCP server then it declares that it is 
 * a server by calling tcp_accept() and then tcp_listen().
 * *******************************************************************/
TCP_T* tcp_new(u32_t ipaddr, u16_t port, err_t* err)
{
  TCP_T *tcp_c = NULL;
  struct NETIF_S *net_adapter;

  net_adapter = netif_ip_route(ipaddr);
  if( net_adapter != NULL )
  {
    tcp_c = tcp_alloc( net_adapter, TCP_PERSISTENT);
    if( tcp_c != NULL)
    {
      tcp_c->local_port = (port != 0)?port:tcp_new_port( net_adapter );
      tcp_c->local_ip = ipaddr;
      T_DEBUGF(TCP_DEBUG,("%s#%d: bound to port %d\r\n",net_adapter->name, tcp_c->local_port, tcp_c->local_port));
      *err = ERR_OK;
    }
    else
    {
      T_ERROR(("Cannot create new TCP tcp_c. Increase MAX_TCP (>%d) or free any unused client connection with tcp_delete()\r\n",MAX_TCP));
      *err = ERR_TCP_MEM;
    }
  }
  else
  {
    T_ERROR(("%s : Cannot find the adapter linked to the IP address %ld.%ld.%ld.%ld\r\n",net_adapter->name, (ipaddr >> 24) & 0xff, (ipaddr >> 16) & 0xff, (ipaddr >> 8) & 0xff, ipaddr & 0xff));
    *err = ERR_VAL;
  }

  return tcp_c;
}

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
void tcp_arg(TCP_T *tcp_c, void *arg)
{
  tcp_c->callback_arg = arg;
}

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
void tcp_recv(TCP_T *tcp_c,
   err_t (* recv)(void *arg, TCP_T *tcp_c, void* data, u32_t data_length))
{
  tcp_c->recv = recv;
}

/*!
 * Function name: tcp_accept
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param accept : [in] accept callback.
 * \brief Specify the function that a child connection calls when a Server creates it.
 * A peer device client connects to a cIPS TCP server. The TCP server creates a child controller.
 * But the child is not connected to the application. tcp_accept() links 
 * the child to the application. The application configures the child connection.
 * To do so, it will call tcp_recv(), tcp_check_connection() and tcp_closed() in tcp_accept().
 * *******************************************************************/
void tcp_accept(TCP_T *tcp_c, err_t (* accept)(void *arg, TCP_T *newtcp_c))
{
  tcp_c->accept = accept;
}

/*!
 * Function name: tcp_check_connection
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param periodic_connection_check : [in] periodic_connection_check callback. 
 * If set to NULL then the TCP core check whether the connection is alive.
 * \param nb_of_500ms : [in] It is a counter. The application must 
 * call tcp_timer() every 500ms. The application decides how often it 
 * wants to check whether the tcp_c's connection has been inactive.
 * The application checks the inactivity every "nb_of_500ms x 500" ms.
 * \brief Set the function that the application uses to specify what to do if a connection has been inactive.
 * \note If a TCP connection connects a peer device to cIPS and if there is no traffic between the two.
 * Either the connection is broken or it is the normal state of operation between the peer device and the application not to exchange messages.
 * cIPS cannot make the difference between the two situations but the application can.
 * So cIPS counts the inactivity time and notifies the application (through the periodic_connection_check() callback).
 * The application can decide to close the connection, to test the connection or to do nothing.
 * *******************************************************************/
void tcp_check_connection(TCP_T *tcp_c, err_t (*periodic_connection_check)(void *arg, TCP_T *tcp_c), u32_t nb_of_500ms)
{
  tcp_c->periodic_connection_check = periodic_connection_check;
  tcp_c->timer = 0;
  tcp_c->nb_of_500ms = nb_of_500ms;
  tcp_c->counter_of_500ms = 0;
}

 /*!
 * Function name: tcp_closed
 * \return nothing.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param closed : [in] Closed callback.
 * \brief CIPS closes a connection. It notifies the application.
 * tcp_closed() sets the application function that cIPS uses to notify 
 * the application that the connection has just been closed.
 * *******************************************************************/
void tcp_closed(TCP_T *tcp_c, err_t (* closed)(void *arg, TCP_T *tcp_c, err_t err))
{
  tcp_c->closed = closed;
}

/*!
 * Function name: tcp_build_data_ethernet_frame
 * \return ERR_OK or ERR_VAL .
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param segment : [in/out] TCP segment containing the outgoing ethernet frame.
 * \param pdata : [in] Pointer to the application data.
 * \param app_len : [in/out] Application data length.
 * \param control_bits : [in] TCP flags.
 * \brief Build a TCP frame.
 * *******************************************************************/
static err_t tcp_build_data_ethernet_frame (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment,
       const u8_t* const pdata,const u32_t app_len, const u8_t control_bits)
{
  TCP_HEADER_T *tcphdr;
  err_t err = ERR_OK;
  u8_t* frame = segment->frame;

  //Fill in app data part
  if (pdata != NULL)
  {
    tcp_memcpy(frame + sizeof(TCP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T), pdata, app_len);
  }

  // Build TCP header :  only update fields that are not constant
  tcphdr = (TCP_HEADER_T *)(frame + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));
  tcphdr->seqno = htonl(tcp_c->local_seqno);
  tcphdr->ackno = htonl(tcp_c->remote_seqno);
  TCP_SET_HEADER_LENGTH(tcphdr, sizeof(TCP_HEADER_T));
  TCP_SET_FLAGS(tcphdr, control_bits);
  tcphdr->windowsize = htons(tcp_c->local_wnd);/* advertise our receive window size in this TCP segment */
  tcphdr->chksum = 0; //reset checksum (because the buffer is not erased before being reused)
  tcphdr->urgent_ptr = 0;
  //no options

  {
    u32_t checksum;
    checksum = eth_build_pseudo_header( tcp_c->remote_ip, tcp_c->local_ip, app_len , sizeof(TCP_HEADER_T), IP_TCP);
    checksum += ip_checksum((const u16_t*)tcphdr, sizeof(TCP_HEADER_T) + app_len); // length contains the standard header + the application data
    //Fold 32-bit sum to 16 bits and add carry
    while( checksum >> 16) {
      checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }
    tcphdr->chksum = (~((u16_t)checksum)); //checksum is the one's complement of the calculated ckecksum. Note: checksum and tcphdr->chksum are big endian.
  }
  if( tcphdr->chksum == 0 ) // chksum zero must become 0xffff, as zero means 'no checksum' 
  {
    tcphdr->chksum = (u16_t)0xFFFF;
  }

  //Fill in IP part
  (void)eth_build_ip_request( tcp_c->remote_ip, tcp_c->local_ip,
    frame + sizeof(ETHER_HEADER_T), app_len + sizeof(TCP_HEADER_T), IP_TCP, segment->frame_initialized);

  return err;
}

/*!
 * Function name: tcp_send_control
 * \return ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param segment : [in/out] TCP segment containing the outgoing ethernet frame.
 * \param control_bits : [in] TCP flags.
 * \param options : [in] TCP options.
 * \param options_length : [in] TCP options length in bytes.
 * \brief Build and send a TCP control frame (SYN, ACK, RST...).
 * *******************************************************************/
static err_t tcp_send_control (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment,
       const u8_t control_bits, const u8_t* const options, const u8_t options_length)
{
  err_t err;

  //1.Build the ethernet frame
  err = tcp_build_control_ethernet_frame (tcp_c, segment, control_bits, options, options_length);
  if(!err)
  {
    //2.Send the frame to the network
    err = netif_send(tcp_c->netif, segment->frame, ETH_IP_TCP_HEADER_SIZE+options_length);
  }
  return err;
}

/*!
 * Function name: tcp_reset
 * \return ERR_RST.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param segment : [in/out] TCP segment containing the outgoing ethernet frame.
 * \param func : [in] Calling function name.
 * \param line : [in] Calling function line.
 * \brief descriptions: When no more segments are available then reset 
 * the connection and print out a trace.
 * *******************************************************************/
static err_t tcp_reset (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment, const s8_t* const func, const u32_t line)
{
  err_t err;

  //1.Build the ethernet frame and send the frame to the network
  err = tcp_send_control(tcp_c, segment, TCP_RST, NULL, 0);

  //2. Print out a trace
  T_ERROR( ("ERR_RST : connection reset. %s @ %ld\r\n",func, line));
  err = tcp_store_error( ERR_RST, tcp_c, func, line);

  //3. Close tcp_c
  tcp_c->state = CLOSED;
  (void)tcp_remove(&(tcp_c->netif->tcp_active_cs), tcp_c);
  (void)tcp_remove(&(tcp_c->netif->tcp_server_cs), tcp_c);
  if(tcp_c->closed) //Report to the application.
  { (void)tcp_c->closed( tcp_c->callback_arg, tcp_c, err );}

  return err;
}

/*!
 * Function name: tcp_build_control_ethernet_frame
 * \return ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param segment : [in/out] TCP segment containing the outgoing ethernet frame.
 * \param control_bits : [in] TCP flags.
 * \param options : [in] TCP options.
 * \param options_length : [in] TCP options length in bytes.
 * \brief Build a TCP control frame (SYN, ACK, RST...).
 * \note A TCP control frame has no data.
 * *******************************************************************/
err_t tcp_build_control_ethernet_frame (TCP_T* const tcp_c, TCP_SENDING_SEG_T* const segment,
       const u8_t control_bits, const u8_t* const options, const u8_t options_length)
{
  TCP_HEADER_T *tcphdr;
  err_t err = ERR_OK;
  u8_t* frame = segment->frame;

  // Build TCP header :  only update the fields that are not constant
  tcphdr = (TCP_HEADER_T *)(frame + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));
  tcphdr->seqno = htonl(tcp_c->local_seqno);
  tcphdr->ackno = htonl(tcp_c->remote_seqno);
  TCP_SET_FLAGS(tcphdr, control_bits);
  tcphdr->windowsize = htons(tcp_c->local_wnd);// advertise our receive window size in this TCP segment
  tcphdr->chksum = 0; //reset checksum (because the buffer is not erased before being reused)
  tcphdr->urgent_ptr = 0;
  // Copy the options into the header, if they are present.
  if (options == NULL)
  { TCP_SET_HEADER_LENGTH(tcphdr, sizeof(TCP_HEADER_T));}
  else 
  { 
    TCP_SET_HEADER_LENGTH(tcphdr, sizeof(TCP_HEADER_T) + options_length);
    tcp_memcpy(frame + sizeof(TCP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T), options, options_length);
  }

  {
    u32_t checksum;
    checksum = eth_build_pseudo_header( tcp_c->remote_ip, tcp_c->local_ip, options_length, sizeof(TCP_HEADER_T), IP_TCP);
    checksum += ip_checksum((const u16_t*)tcphdr, sizeof(TCP_HEADER_T) + options_length); // length contains the standard header + the options
    //Fold 32-bit sum to 16 bits and add carry
    while( checksum >> 16) {
      checksum = (checksum & 0xFFFF) + (checksum >> 16);
    }

    tcphdr->chksum = (~((u16_t)checksum)); //checksum is the one's complement of the calculated ckecksum. Note: checksum and tcphdr->chksum are big endian.
  }
  if( tcphdr->chksum == 0 ) // chksum zero must become 0xffff, as zero means 'no checksum' 
  {
    tcphdr->chksum = (u16_t)0xFFFF;
  }

  //3.Fill in IP part
  (void)eth_build_ip_request( tcp_c->remote_ip, tcp_c->local_ip,
    frame + sizeof(ETHER_HEADER_T), options_length + sizeof(TCP_HEADER_T), IP_TCP, segment->frame_initialized);

  return err;
}

/*!
 * Function name: tcp_need_acknowlegment
 * \return nothing
 * \param segment : [in/out] List of active TCP controllers.
 * \param app_AND_option_len : [in] App data length AND option length.
 * \param seqno : [in] Sepuence number expected.
 * \brief Declare that a frame needs an ACK. The consequence 
 * is that cIPS can retransmit the frame.
 * \note To be retransmitted, ->len, ->state and ->ack_no are necessary.
 * So call segment_change_state() in association to that function.
 * *******************************************************************/
static void tcp_need_acknowledgment (TCP_SENDING_SEG_T* segment, const u32_t app_AND_option_len, const u32_t seqno)
{
  segment->len = app_AND_option_len + sizeof(TCP_HEADER_T) + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T);
//  segment->state = TCP_SEG_UNACKED; //This is done in "segment_change_state"
  segment->ack_no = seqno;
  T_DEBUGF(TCP_DEBUG, ("Registers ACK#0x%x in database\r\n", (unsigned int)seqno));
  return;
}

/*!
 * Function name: tcp_create_child
 * \return nothing ERR_TCP_MEM, ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER
 * \param tcp_c : [in] Server tcp_c.
 * \param ip_frame : [in] frame beginning with the IP header.
 * \brief A peer device client connects to a cIPS TCP server. The TCP server 
 * creates a child controller with tcp_create_child().
 * \note tcp_create_child() calls tcp_c->accept() so that the 
 * application can customize and work with the new connection.
 * *******************************************************************/
static err_t tcp_create_child(TCP_T *tcp_c, u8_t* ip_frame)
{
  TCP_T *ntcp_c; //Child tcp_c
  u32_t options;
  err_t err = ERR_OK;
  IP_HEADER_T* iphdr = (IP_HEADER_T*) ip_frame;
  TCP_HEADER_T* tcphdr = (TCP_HEADER_T *)(ip_frame + sizeof(IP_HEADER_T));

  T_ASSERT(("%s#%d Register a callback with tcp_accept() on TCP controller 0x%lx\r\n",__func__, __LINE__, (u32_t)tcp_c), tcp_c->accept != NULL);
  (void)tcp_remove_server_child_cs(tcp_c->netif); //garbage collector

  ntcp_c = tcp_alloc( tcp_c->netif, TCP_NON_PERSISTENT);
  if(ntcp_c != NULL)
  {
    ntcp_c->local_ip = tcp_c->local_ip;
    ntcp_c->remote_ip = ntohl(iphdr->source_addr);
    ntcp_c->state = SYN_RCVD;
    ntcp_c->callback_arg = tcp_c->callback_arg;
    ntcp_c->local_port = tcp_c->local_port;
    ntcp_c->remote_port = ntohs(tcphdr->source_port); //tcphdr points to the incoming frame which is big endian.
    ntcp_c->remote_seqno = ntohl(tcphdr->seqno)+1;
    ntcp_c->remote_wnd = ntohs(tcphdr->windowsize);
    ntcp_c->remote_mss = 0;
    ntcp_c->nb_of_500ms = tcp_c->nb_of_500ms;
    ntcp_c->type = TCP_NON_PERSISTENT; //type: TCP_PERSISTENT or TCP_NON_PERSISTENT

    { //Initialize fields that are staying constant for the life of the connection
      ETHER_HEADER_T* ethhdr = (ETHER_HEADER_T*) (ip_frame - sizeof(ETHER_HEADER_T));
      (void)segment_init_connection (ntcp_c, ethhdr->source_addr);
    }

    err = tcp_c->accept(ntcp_c->callback_arg,ntcp_c);
    if(!err)
    {
      tcp_register(&(tcp_c->netif->tcp_active_cs), ntcp_c);
      if(TCP_GET_HEADER_LENGTH(tcphdr) > sizeof(TCP_HEADER_T))
      {
        ntcp_c->remote_mss = tcp_parse_options(ip_frame+ sizeof(IP_HEADER_T) + sizeof(TCP_HEADER_T));/* Parse any options in the SYN. */
        if( ntcp_c->remote_mss >= TCP_MTU) { ntcp_c->remote_mss = TCP_MTU; } //cap to what the adapter can send.
      }
      // Build an MSS option.
      options = tcp_format_max_segment_size_option(ntcp_c->local_mss);
      // Send a SYN|ACK together with the MSS option.
      err = tcp_send_control (ntcp_c, &ntcp_c->control_segment, TCP_SYN|TCP_ACK, (u8_t *)&options, sizeof(options));
      ntcp_c->local_seqno++; //Note: increment for TCP_SYN or TCP_FIN but not for TCP_ACK
    }
  }
  else
  {
    T_ERROR( ("tcp_listen_input: could not allocate TCP controller\r\n"));
    err =tcp_store_error( ERR_TCP_MEM, tcp_c, __func__, __LINE__);
  }
  return err;
}


/*!
 * Function name: tcp_remove_server_child_cs
 * \return nothing
 * \param net_adapter : [in] Adapter of interest.
 * \brief This is a kind of garbage collector. CIPS reuses
 * the controllers. tcp_remove_server_child_cs() identifies the 
 * controller that are not used anymore. The criteria is the flag 
 * "TCP_SERVER_CHILD" and the state "CLOSED".
 * *******************************************************************/
static void tcp_remove_server_child_cs( struct NETIF_S* net_adapter )
{
  TCP_T *tcp_c_i;
  int i;
  TCP_T* tcp_c_list = net_adapter->tcp_c_list;

  for( i = 0; i < MAX_TCP; i++)
  {
    tcp_c_i = &(tcp_c_list[i]);
    if( (tcp_c_i->type == TCP_NON_PERSISTENT) && (tcp_c_i->state == CLOSED))
    {
      tcp_c_i->id = UNUSED;
    }
  }
}

/*!
 * Function name: tcp_order_active_list
 * \return nothing
 * \param tcp_active_cs : [out] List of active tcp controllers.
 * \param net_adapter : [in] Adapter of onterest.
 * \brief Re-order the list of TCP clients per adapter.
 * A network adapter supports several TCP clients. When the peer device 
 * sends a TCP frame to an adapter, the adapter forwards the frame to cIPS,
 * then cIPS browses the list of clients in order to find a match.
 * The browsing is faster if the TCP controllers used are at the 
 * beginning of the list. tcp_order_active_list() puts the TCP
 * controller used at the beginning of the list.
 * *******************************************************************/
static void tcp_order_active_list( TCP_T** tcp_active_cs, const struct NETIF_S* const net_adapter)
{
  TCP_T *tcp_c_i;
  int i;
  int prec;
  TCP_T* tcp_c_list = (TCP_T*)net_adapter->tcp_c_list;

  //look for first elt.
  prec = UNUSED;
  for( i = 0; i < MAX_TCP; i++)
  {
    tcp_c_i = &(tcp_c_list[i]);
    if( (tcp_c_i->id != UNUSED) && (tcp_c_i->state != CLOSED))
    {
      prec = i; //current elt becomes precedent elt.
      i = MAX_TCP; // exit loop
    }
  }

  //Set the "active list" to the first elt.
  *tcp_active_cs = (prec != UNUSED) ? &(tcp_c_list[prec]) : NULL;

  //build active connection list.
  for( i = prec+1; i < MAX_TCP; i++)
  {
    tcp_c_i = &(tcp_c_list[i]);
    if( (tcp_c_i->id != UNUSED) && (tcp_c_i->state != CLOSED))
    {
      tcp_c_list[prec].next = tcp_c_i;
      prec = i; //current elt becomes precedent elt.
    }
  }
  //last elt
  tcp_c_list[prec].next = NULL;

#ifdef TCP_DEBUG
  {
  u32_t unused =0;
  u32_t used_and_closed =0;
  u32_t used = 0;
  int max_display = 7; //Note: Restrict the display to "max_display" in order to limit traces taken by this function.

  if( MAX_TCP < max_display) { max_display = MAX_TCP;}

  T_DEBUGF(TCP_DEBUG, ("TCP pbs : %d elts [index,used ,type]: \r\n",MAX_TCP));
  for( i = 0; i < max_display; i++)
  {
    T_DEBUGF(TCP_DEBUG, ("[%d,",i));
    tcp_c_i = &(tcp_c_list[i]);
    if( tcp_c_i->id == UNUSED)
    {
       unused++;
       T_DEBUGF(TCP_DEBUG, (" UNUSED,"));
    }
    else
    {
       T_DEBUGF(TCP_DEBUG, (" USED#%d-%d (",tcp_c_i->local_port,tcp_c_i->remote_port));
       tcp_debug_print_state(tcp_c_i->state);
       T_DEBUGF(TCP_DEBUG, ("),"));
      if(tcp_c_i->state == CLOSED)
      {
        used_and_closed++;
      }
      used++;
    }
    if( tcp_c_i->type == TCP_PERSISTENT)
    { T_DEBUGF(TCP_DEBUG, (" PERS]\r\n"));}
    else
    { T_DEBUGF(TCP_DEBUG, (" UNPERS]\r\n"));}
  }
    T_DEBUGF(TCP_DEBUG, ("TCP controllers: unused=%ld used_and_closed=%ld used=%ld\r\n",unused,used_and_closed, used));
  }
#endif //TCP_DEBUG
  return;
}

/*!
 * Function name: tcp_register
 * \return nothing
 * \param tcp_active_cs : [out] List of active controllers.
 * \param tcp_c : [in] tcp_c of interest, tcp_c->id must be set to a different value of
 * UNUSED to register the tcp_c.
 * \brief Add the new server to the list of active TCP connections.
 * *******************************************************************/
static void tcp_register( TCP_T** tcp_active_cs,  TCP_T* tcp_c )
{
  (void) tcp_order_active_list( tcp_active_cs, tcp_c->netif);
}

/*!
 * Function name: tcp_remove
 * \return nothing
 * \param tcp_active_cs : [out] List of active conrollers.
 * \param tcp_c : [in] tcp_c of interest, tcp_c->state must be set to CLOSED to remove the tcp_c.
 * \brief Remove the new server from the list of Tcp listeners by rebuilding the list.
 * *******************************************************************/
static void tcp_remove( TCP_T** tcp_active_cs,  TCP_T* tcp_c )
{
  tcp_c->state = CLOSED; //Condition to reorder the active list.
  (void)segment_init_resource(tcp_c); //reset segments (note: this is also done when the socket is re-allocated)
  (void) tcp_order_active_list( tcp_active_cs, tcp_c->netif);
}

/*!
 * Function name: tcp_store_error
 * \return nothing
 * \param err : [in] error number.
 * \param tcp_c : [in/out] tcp_c sending the error.
 * \param function_name : [in] function name where the error is happening.
 * \param line_number : [in] line number where the error is happening.
 * \brief Format a string to report a TCP error.
 * *******************************************************************/
static err_t tcp_store_error( const err_t err, TCP_T* const tcp_c, const s8_t* const function_name, const u32_t line_number)
{
  ERROR_REPORT_T* last_error = &(tcp_c->netif->last_error);
  last_error->err_nb = err;
  snprintf(last_error->formated_error,MAX_FORMATED_ERROR_SIZE,"%s#%d-%d: TCP error: %s (fct %s #%ld)",tcp_c->netif->name, tcp_c->local_port, tcp_c->remote_port, decode_err(err), function_name, line_number);
  return err;
}

/*!
 * Function name: malloc_tcp_c
 * \return free_tcp_c: a pointer to a new tcp_c.
 * \param net_adapter : [in/out] network adapter.
 * \brief A network adapter has a list of TCP controller. Some are used,
 * some are not or not yet. When CIPS creates a TCP controller on an 
 * adpater it calls malloc_tcp_c() in order to find a TCP controller 
 * available. So calls malloc_tcp_c() books a tcp_c in the list of TCP 
 * controllers and returns a reference to it.
 * *******************************************************************/
static TCP_T* malloc_tcp_c( struct NETIF_S* net_adapter)
{
  TCP_T *tcp_c_i;
  TCP_T* free_tcp_c = NULL;
  TCP_T* tcp_c_list = net_adapter->tcp_c_list;
  int i=0;

  for( i = 0; i < MAX_TCP; i++)
  {
    tcp_c_i = &(tcp_c_list[i]);
    if( tcp_c_i->id == UNUSED)
    {
      free_tcp_c = tcp_c_i;
      tcp_c_i->id = i;
      i = MAX_TCP; // exit loop
    }
  }
  return free_tcp_c;
}

/*!
 * Function name: segment_init_resource
 * \return nothing.
 * \param tcp_c: [in/out] TCP controller of interest.
 * \brief Outgoing data can be larger than the TCP payload. Segments 
 * exits to split the data on multiple TCP frames.
 * segment_init_resource() initializes the segment of a TCP controller.
 * *******************************************************************/
static void segment_init_resource(TCP_T* tcp_c)
{
  int i;
  for ( i = 0; i <MAX_TCP_SEG ; i++)
  {
    tcp_c->segment[i].state = TCP_SEG_UNUSED;
    tcp_c->segment[i].frame_initialized = FALSE;
    tcp_c->segment[i].retransmission_timer_slice = 0;
  }
  tcp_c->seg_nb[TCP_SEG_UNUSED] = MAX_TCP_SEG;
  tcp_c->seg_nb[TCP_SEG_UNSENT] = 0;
  tcp_c->seg_nb[TCP_SEG_UNACKED] = 0;
}

/*!
 * Function name: segment_init_connection
 * \return ERR_MAC_ADDR_UNKNOWN, ERR_DEVICE_DRIVER.
 * \param tcp_c : [in/out] tcp_c of interest.
 * \param dest_mac_addr : [in] Destination MAC address.
 * \brief cIPs limits the calculation. A communication between cIPS an a peer
 * device is an agreement. It is an agreement on the IP addresses and port numbers.
 * This agreement never changes so it is constant. segment_init_connection()
 * initializes the fields that are staying constant for the life of the 
 * connection for each segment.
 * *******************************************************************/
static void segment_init_connection (TCP_T* const tcp_c, const u8_t* const dest_mac_addr)
{
  TCP_HEADER_T *tcphdr;
  u8_t* frame = tcp_c->control_segment.frame;

  //Init the "segment of control" with everything that is known and constant: i.e. 
  // IP_HEADER_T:version_head_length,  IP_HEADER_T:type_of_service, IP_HEADER_T:id,
  // IP_HEADER_T:fragment_offset_field, IP_HEADER_T:time_to_live, IP_HEADER_T:protocol, IP_HEADER_T:source_addr, IP_HEADER_T:dest_addr,
  // TCP_HEADER_T:source_port, TCP_HEADER_T:dest_port. That 10 concatenated longs.

  u32_t dest_ip_or_gateway = arp_get_route(tcp_c->remote_ip, tcp_c->netif->gateway_addr,tcp_c->netif->netmask);

  //Fill in ethernet part
  (void)eth_build_frame( dest_mac_addr, tcp_c->netif->mac_address, dest_ip_or_gateway, tcp_c->local_ip, frame, ETH_ETHERNET);

  //Fill in IP part
  (void) ip_set_constant_fields( tcp_c->remote_ip, tcp_c->local_ip, frame + sizeof(ETHER_HEADER_T), IP_TCP);

  //Build TCP header.
  tcphdr = (TCP_HEADER_T *)(frame + sizeof(IP_HEADER_T) + sizeof(ETHER_HEADER_T));
  tcphdr->source_port = htons(tcp_c->local_port);
  tcphdr->dest_port = htons(tcp_c->remote_port);

  tcp_c->control_segment.frame_initialized = TRUE;

  //Once the "control segment" is initialized, copy it to each "data segment"
  //Note: the copy is optimized by copying 32 bits at a time. They are 10 times 32 bits from the fisrt item (ETHER_HEADER_T:destination_addr) to the last (TCP_HEADER_T:dest_port).
  {
    #define CONSTANT_HEADER_LENGTH 10
    int i;
    int j;
    u32_t* src;
    u32_t* dst;
    i = 0;
    do{
      j= 0;
      src = (u32_t*)frame;
      dst = (u32_t*)tcp_c->segment[i].frame;
      do{
        *dst++ = *src++;
      }while( ++j < CONSTANT_HEADER_LENGTH);
      tcp_c->control_segment.frame_initialized = TRUE;
    } while(++i<MAX_TCP_SEG);
  }

  return;
}

/*!
 * Function name: segment_get_first
 * \return a pointer to the segment found. NULL otherwise.
 * \param tcp_c: [in/out] TCP controller of interest
 * \param state: [in] state of interest
 * \brief CIPS uses segments to send TCP frames to a peer device. There are
 * several kind of segments: some wait for a peer device acknowledgment, some 
 * are available. segment_get_first() looks for the first available.
 * *******************************************************************/
static TCP_SENDING_SEG_T * segment_get_first( TCP_T* tcp_c, const seg_state state)
{
  u32_t i = 0;
  TCP_SENDING_SEG_T *elt = tcp_c->segment;

  while ( (elt->state != state) && (i != MAX_TCP_SEG) )
  {
    elt++;
    i++;
  }
  return (i != MAX_TCP_SEG)?elt:NULL;
}

/*!
 * Function name: segment_get_first_unused_after_unacked
 * \return a pointer to the segment found. NULL otherwise.
 * \param tcp_c: [in/out] TCP controller of interest
 * \param state: [in] state of interest
 * \brief CIPS uses segments in order to send messages larger than the TCP payload
 * or to accumulate tcp_write() if the network is slower than the CPU.
 * Most of the time, the network is faster than the flow of tcp_write().
 * But when it is not then cIPS must keeps the order of the frames.
 * To do so, if looks for the first UNUSED segment after the last UNACKED (which is the last UNACKED).
 * *******************************************************************/
static TCP_SENDING_SEG_T * segment_get_first_unused_after_unacked( TCP_T* tcp_c, const seg_state state)
{
  seg_state last_state = TCP_SEG_UNUSED;
  u32_t i = 0;
  TCP_SENDING_SEG_T *elt = tcp_c->segment;

  //TCP_SEG_UNUSED
  while ( (!((last_state == TCP_SEG_UNACKED) && (elt->state == TCP_SEG_UNUSED))) && (i != MAX_TCP_SEG))
  {
    last_state = elt->state;
    elt++;
    i++;
  }

  return (i != MAX_TCP_SEG)?elt:NULL;
}
/*!
 * Function name: segment_change_state
 * \return nothing
 * \param tcp_c: [in/out] TCP tcp_c of interest.
 * \param elt: [out] sending segment of interest.
 * \param new_state:  [in] state of interest.
 * \brief Change the state of a segment and updates the segment counters.
 * \brief CIPS uses segments to send TCP frames to a peer device.
 * There are several kind of segments: see the "seg_state" enumeration.
 * segment_change_state() changes the state of a segment and updates 
 * the segment counters. The segment counters manage the resource.
 * *******************************************************************/
static void segment_change_state(TCP_T* tcp_c, TCP_SENDING_SEG_T *elt, const seg_state new_state)
{
  (tcp_c->seg_nb[elt->state])--;
  elt->state = new_state;
  (tcp_c->seg_nb[new_state])++;

  T_DEBUGF(TCP_DEBUG, ("%s#%d: Segments: ",tcp_c->netif->name, tcp_c->local_port));
  T_DEBUGF(TCP_DEBUG, ("unused=%ld, unsent=%ld unacked=%ld\r\n",tcp_c->seg_nb[TCP_SEG_UNUSED],tcp_c->seg_nb[TCP_SEG_UNSENT],tcp_c->seg_nb[TCP_SEG_UNACKED]));

  return;
}

/*!
 * Function name: tcp_lookup_segment_by_acknowledge_no
 * \return nothing. 
 * \param tcp_c: [in/out]TCP tcp_c of interest.
 * \param ack_no: [in] acknowledge number to find in database.
 * \brief cIPS sends a TCP frame to a peer device. Then the peer device acknowledges 
 * the frame to cIPS. cIPS holds the frame in case it needs to retransmit it.
 * If cIPs does not receive an acknowledgment then it retransmits the frame.
 * If cIPS receives an acknowlegement then it does not hold the frame 
 * anymore. tcp_lookup_segment_by_acknowledge_no() identifies the frame held
 * by its acknowledgment number and frees it.
 * *******************************************************************/
static void tcp_lookup_segment_by_acknowledge_no(TCP_T* const tcp_c, const u32_t ack_no)
{
  u32_t i;
  T_DEBUGF(TCP_DEBUG, ("%s#%d: TCP recv segment to ACK : #0x%x ->",tcp_c->netif->name, tcp_c->local_port, (unsigned int)ack_no));

  //Sometimes, the peer device skips the aknowledgment of a segment with an ACK flag but sends its next
  //frame with its acknowlegment number incremented. If the peer device increments the acknowlegment number 
  //then it means that it has received the segment.
  //CIPS keeps the list of the segments not acknowledged yet. When it receives an 
  //aknowledgment, it aknowledges the corresponding segment and the previous ones.
  //CIPS frees the segments so that it could use them to hold other segments.
  i =0;
  do {
    if((tcp_c->segment[i].state == TCP_SEG_UNACKED) && 
       ((tcp_c->segment[i].ack_no <= ack_no) || // cIPS acknowleges segments with an ack_no lower than the one received.
        ((ack_no < tcp_c->last_ack_no) && //But the received ack_no rolls over when it is bigger than 0xFFFFFFFF. last_ack_no handles the rollover situation.
         (tcp_c->segment[i].ack_no >= tcp_c->last_ack_no))))
    {
      (void)segment_change_state( tcp_c, &(tcp_c->segment[i]), TCP_SEG_UNUSED);
    }
    i++;
  } while ((tcp_c->seg_nb[TCP_SEG_UNACKED]) && (i< MAX_TCP_SEG)); //Note: segment_change_state() decrements tcp_c->seg_nb[TCP_SEG_UNACKED]
  tcp_c->last_ack_no = ack_no; //update in case if the next "ack_no" rolls over.

#ifdef TCP_DEBUG
  if(i== MAX_TCP_SEG)
  {
    T_DEBUGF(TCP_DEBUG, (" ACK#0x%x NOT found in database or already ACKed\r\n", (unsigned int)ack_no));
  }
#endif //TCP_DEBUG

#ifdef TCP_DEBUG
  T_DEBUGF(TCP_DEBUG, ("%s#%d: TCP: %ld ACKs waiting : ",tcp_c->netif->name, tcp_c->local_port, tcp_c->seg_nb[TCP_SEG_UNACKED]));
  for( i = 0 ; i < MAX_TCP_SEG; i++)
  {
    if( tcp_c->segment[i].state == TCP_SEG_UNACKED )
    {
        T_DEBUGF(TCP_DEBUG, ("0x%lx ",tcp_c->segment[i].ack_no));
    }
  }
  T_DEBUGF(TCP_DEBUG, ("\r\n"));
#endif //TCP_DEBUG

  return;
}

/*!
 * Function name: tcp_new_port
 * \return new port number
 * \param net_adapter : [in/out] tcp_c of interest.
 * \brief A port is necessary for a TCP connection. If the application
 * does not specify one then cIPS assigns one. tcp_new_port() looks for
 * a free port.
 * \note Return the lowest port number (or the oldest allocated) if no
 * more ports are available.
 * *******************************************************************/
static u16_t tcp_new_port( struct NETIF_S* net_adapter)
{
#ifndef TCP_LOCAL_PORT_RANGE_START
#define TCP_LOCAL_PORT_RANGE_START 0x61A8
#define TCP_LOCAL_PORT_RANGE_END   0x70ff
#endif
  int i;
  u16_t port = TCP_LOCAL_PORT_RANGE_START;
  TCP_T* tcp_c_list = (TCP_T*)net_adapter->tcp_c_list;

  T_ASSERT(("%s#%d More potential TCP connections than allowed port numbers.\r\n",__func__, __LINE__), MAX_TCP < TCP_LOCAL_PORT_RANGE_END-TCP_LOCAL_PORT_RANGE_START);

  for( i = 0; i < MAX_TCP; i++){
    if( tcp_c_list[i].local_port == port){
      //The port is used so increment the port and check again from the beginning.
      port++;
      i = 0;
    }
  }

  return port;
}

/*!
 * Function name: tcp_memcpy
 * \return a pointer to the destination array.
 * \param output : [in] Array to copy into.
 * \param input : [in]  Array to be copied.
 * \param length : [in] Number of bytes to copy.
 * \brief Copy each elt of one array to another one.
 * *******************************************************************/
static u8_t* tcp_memcpy(u8_t* output, const u8_t* input, const u32_t length)
{
  u32_t l = length;
  while(l--)
  {
    *output++ = *input++;
  }
  return output;
}
/*!
 * Function name: tcp_parse_options
 * \return the remote maximum segment size.
 * \param option : [in] pointer to the option field.
 * \brief Parses the TCP options to get the Max segment size.
 * *******************************************************************/
static u32_t tcp_parse_options(const u8_t* const option)
{
  #define END_OF_OPTION_LIST_KIND 0x00 //See RFC793
  #define NO_OPERATION_KIND 0x01 //See RFC793
  #define MAX_SEG_SIZE_KIND 0x02 //See RFC793
  u16_t mss = 0;
  switch(*option)
  {
    case MAX_SEG_SIZE_KIND:
      //Get the Max segment size.
      //Note: The MSS is the last 2 bytes of the 4 bytes of the option field.
      //"Option" points to the incoming frame which is big endian.
      //So cIPS gets the MSS field by pointing to the last 2 bytes of the "option"
      //field of the big endian tcp frame.
      //It gets a big endian value. If the platform is little endian, it gets the
      //the right value in the register by applying ntohs() .
      mss = ntohs(*(u16_t*)(option + sizeof(u16_t))); //get the Max segment size.
    break;
    case END_OF_OPTION_LIST_KIND:
    case NO_OPERATION_KIND:
    default:
      mss = 0;
    break;
  };
  return mss;
}

/*!
 * Function name: tcp_format_max_segment_size_option
 * \return the remote maximum segment size (in big endian).
 * \param mss : [in] local naximum segment size.
 * \brief Generates the "options" field.
 * *******************************************************************/
static u32_t tcp_format_max_segment_size_option(const u16_t mss)
{
  u32_t options;
  const u32_t MAX_SEG_SIZE_OPTION_MASK = 0x2040000; //See RFC793
  const u32_t MAX_SEG_SIZE_MASK = 0xFFFF;
  options = MAX_SEG_SIZE_OPTION_MASK | (mss & MAX_SEG_SIZE_MASK);

  return htonl(options);
}

#if TCP_DEBUG
static void tcp_debug_print_state(enum tcp_state s)
{
  T_DEBUGF(TCP_DEBUG, ("state: "));
  switch (s) {
  case CLOSED:
    T_DEBUGF(TCP_DEBUG, ("CLOSED"));
    break;
 case LISTEN:
   T_DEBUGF(TCP_DEBUG, ("LISTEN"));
   break;
  case SYN_SENT:
    T_DEBUGF(TCP_DEBUG, ("SYN_SENT"));
    break;
  case SYN_RCVD:
    T_DEBUGF(TCP_DEBUG, ("SYN_RCVD"));
    break;
  case ESTABLISHED:
    T_DEBUGF(TCP_DEBUG, ("ESTABLISHED"));
    break;
  case FIN_WAIT_1:
    T_DEBUGF(TCP_DEBUG, ("FIN_WAIT_1"));
    break;
  case FIN_WAIT_2:
    T_DEBUGF(TCP_DEBUG, ("FIN_WAIT_2"));
    break;
  case CLOSE_WAIT:
    T_DEBUGF(TCP_DEBUG, ("CLOSE_WAIT"));
    break;
  case CLOSING:
    T_DEBUGF(TCP_DEBUG, ("CLOSING"));
    break;
  case LAST_ACK:
    T_DEBUGF(TCP_DEBUG, ("LAST_ACK"));
    break;
  case TIME_WAIT:
    T_DEBUGF(TCP_DEBUG, ("TIME_WAIT"));
   break;
  }
}

static void tcp_debug_print_flags(u8_t flags)
{
  if (flags & TCP_FIN) { T_DEBUGF(TCP_DEBUG, ("FIN "));}
  if (flags & TCP_SYN) { T_DEBUGF(TCP_DEBUG, ("SYN "));}
  if (flags & TCP_RST) { T_DEBUGF(TCP_DEBUG, ("RST"));}
  if (flags & TCP_PSH) { T_DEBUGF(TCP_DEBUG, ("PSH "));}
  if (flags & TCP_ACK) { T_DEBUGF(TCP_DEBUG, ("ACK"));}
  if (flags & TCP_URG) { T_DEBUGF(TCP_DEBUG, ("URG "));}
}

static void tcp_debug_print_command(u32_t command)
{
  switch(command)
  {
    case TCP_USER_PASSIVE_OPEN:
      T_DEBUGF(TCP_DEBUG, ("TCP_USER_PASSIVE_OPEN "));
    break;
    case TCP_USER_ACTIVE_OPEN:
      T_DEBUGF(TCP_DEBUG, ("TCP_USER_ACTIVE_OPEN "));
    break;
    case TCP_USER_SEND:
      T_DEBUGF(TCP_DEBUG, ("TCP_USER_SEND "));
    break;
    case TCP_USER_CLOSE:
      T_DEBUGF(TCP_DEBUG, ("TCP_USER_CLOSE "));
    break;
    default:
      T_DEBUGF(TCP_DEBUG, ("does not exist "));
    break;
  }
  return;
}
#endif /* TCP_DEBUG */
