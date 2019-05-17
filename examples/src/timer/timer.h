#ident "@(#) $Id: timer.h,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \namespace  ti
* \file timer.h
* \brief This module offers some timing services like timeout or sleep.
* *******************************************************************/
#ifndef _TIMER_H_
#define _TIMER_H_


//!Timer descriptor
typedef struct {
  u32_t base_address; //<!HW base address
  u32_t timer_number; //<!HW timer number
  u32_t old_counter; //<! old counter for ti_get_timer_event()
  u32_t relative_timeout_counter; //<! Intermediate variable for ti_timeout()
  u32_t elapsed_counter; //<! Intermediate variable for ti_timeout()
  u32_t absolute_timeout_counter;//<! Intermediate variable for ti_timeout()
  u32_t initial_absolute_counter;//<! Intermediate variable for ti_timeout()
  bool_t timeout_started;//<! Indicates that ti_timeout() has started
} TIMER_T;

#ifdef __cplusplus
extern "C" {
#endif
  void ti_init_capture_timer(u32_t base_address, u32_t timer_number);
  void ti_reset_timer_descriptor(TIMER_T* const timer, u32_t base_address, u32_t timer_number);
  u32_t ti_microseconds_to_ticks(u32_t time);
  void ti_get_timer_event(TIMER_T* const timer, u32_t* time, bool_t* tick);
  u32_t ti_get_timer_counter(TIMER_T* const timer);
  void ti_sleep(const TIMER_T* const timer, u32_t duration);
  bool_t ti_timeout(TIMER_T* const timer, const bool_t start, const u32_t timeout);
#ifdef __cplusplus
}
#endif

#endif //_TIMER_H_
