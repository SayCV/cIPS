#ident "@(#) $Id: timer.c,v 1.1 2012/12/06 16:36:26 jdavid Exp $"
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
* \file timer.c
* \brief This module offers some timing services like timeout or sleep.
* *******************************************************************/

// Xilinx Includes
#include "xtmrctr.h" //for XTmrCtr_xxx, XTC_CSR_xxx  

// Application Includes
#include "basic_types.h"
#include "debug_trace.h"
#include "hw_mapping.h"
#include "timer.h"

// External Declarations


// Internal Declarations
static u32_t ti_diff(u32_t old_counter, u32_t counter);

/*!
 * Function name: ti_init_capture_timer
 * \return nothing.
 * \param base_address [in]: HW base address
 * \param timer_number [in]: HW timer number
 * \brief descriptions: Initialize a timer in capture mode.
 * Must be call before any of the other module's methods.
 * *******************************************************************/
void ti_init_capture_timer(u32_t base_address, u32_t timer_number)
{
  // Set the control status register
  // XTC_CSR_ENABLE_TMR_MASK : Enables only the specific timer
  // XTC_CSR_CAPTURE_MODE_MASK : capture mode
  (void) XTmrCtr_SetControlStatusReg(base_address, timer_number,
      XTC_CSR_ENABLE_TMR_MASK | XTC_CSR_CAPTURE_MODE_MASK);

  return;
}

/*!
 * Function name: ti_reset_timer_descriptor
 * \return nothing.
 * \param timer [in]: timer descriptor
 * \param base_address [in]: HW base address
 * \param timer_number [in]: HW timer number
 * \brief descriptions: Initialize a timer descriptor to access the 
 * timeout or sleep functionalities.
 * *******************************************************************/
void ti_reset_timer_descriptor(TIMER_T* const timer, u32_t base_address, u32_t timer_number)
{
  timer->base_address = base_address;
  timer->timer_number = timer_number;
  timer->old_counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number); //<! old counter for ti_get_timer_event()
  timer->elapsed_counter = 0; //<! Intermediate variable for ti_timeout()
  timer->absolute_timeout_counter = 0;//<! Intermediate variable for ti_timeout()
  timer->relative_timeout_counter = 0;//<! Intermediate variable for ti_timeout()
  timer->initial_absolute_counter = 0;//<! Intermediate variable for ti_timeout()
  timer->timeout_started = FALSE;//<! Indicates that ti_timeout() has started
  return;
}

/*!
 * Function name: ti_get_timer_event
 * \return nothing
 * \param timer [in/out]: timer descriptor
 * \param counter [out]: the absolute timer counter number.
 * \param tick [out]: indicate that 10 milliseconds (or MAX_ELAPSED) has elapsed
 * \brief descriptions: 
 * 1) Returns the absolute timer counter number.
 * 2) Indicate that 10 milliseconds (or MAX_ELAPSED) has elapsed
 * *******************************************************************/
void ti_get_timer_event(TIMER_T* const timer, u32_t* counter, bool_t* tick)
{
  static const u32_t MAX_ELAPSED = (u32_t)(XPAR_CPU_CORE_CLOCK_FREQ_HZ/100); //The resolution of event updates is 10ms (= MAX_ELAPSED*COUNTER_PERIOD)
  u32_t diff;

  //Get timer's counts
  *counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);
  //How much counts has elapsed
  diff = ti_diff(timer->old_counter, *counter);
  timer->old_counter = *counter;

  timer->elapsed_counter += diff;
  if (timer->elapsed_counter >= MAX_ELAPSED) { //after 1 milliseconds (or more)
    *tick = TRUE;
    timer->elapsed_counter = 0; //Reset after 1 millisecond
  } else {
    *tick = FALSE;
  }
  return;
}

/*!
 * Function name: ti_get_timer_counter
 * \return the absolute timer counter number
 * \param timer [in/out]: timer descriptor
 * \brief Simply gives the timer count
 * *******************************************************************/
u32_t ti_get_timer_counter(TIMER_T* const timer)
{
  u32_t counter;
  //Get timer's counts
  counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);
  return counter;
}

/*!
 * Function name: ti_sleep
 * \return nothing
 * \param timer :[in] timer desciptor
 * \param duration :[in] amount of time to wait in 10-4 seconds
 * \brief Wait for the duration specified in the parameter
 * *******************************************************************/
void ti_sleep(const TIMER_T* const timer, u32_t duration)
{
  static const u32_t MILLISECONDS_TO_COUNTS = (u32_t)(XPAR_CPU_CORE_CLOCK_FREQ_HZ/10000); //In 0.1 milliseconds = 10-4 seconds
  u32_t counter;
  u32_t target_counter;
  u32_t start_counter;

  start_counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);
  target_counter = start_counter + (duration * MILLISECONDS_TO_COUNTS);

  if( target_counter >= start_counter ){
    do{
      counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);
    } while( counter <= target_counter);
  } else { //target_counter overflew
    do{
      counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);
    } while(( counter <= target_counter) || ( counter >= start_counter));

  }
}

/*!
 * Function name: ti_timeout
 * \return TRUE if the timeout has elapsed, FALSE otherwise
 * \param timer [in/out]: timer descriptor
 * \param start: [in] indicates to start the timer of "timeout" duration
 * \param timeout: [in] duration in MICROseconds
 * \brief Indicate when the amount of time "timeout" has elapsed
 * *******************************************************************/
bool_t ti_timeout(TIMER_T* const timer, const bool_t start, const u32_t timeout)
{
  bool_t elapsed = FALSE;
  u32_t counter;

  counter = XTmrCtr_GetTimerCounterReg(timer->base_address, timer->timer_number);

  if(start){
    timer->initial_absolute_counter = counter;
    timer->relative_timeout_counter = ti_microseconds_to_ticks(timeout);
    timer->timeout_started = TRUE;
  } else if(timer->timeout_started == TRUE){
    u32_t diff;
    diff = ti_diff(timer->initial_absolute_counter, counter);
    if(diff >= timer->relative_timeout_counter){
      elapsed = TRUE;
    }
  }

  return elapsed;
}

/*!
 * Function name: ti_diff
 * \return the difference between two counter values
 * \param old_counter: [in] previous value of the counter
 * \param counter: [in] counter value
 * \brief descriptions: Calculates the difference between two counter values
 * but mainly manages the case when the counter roll back
 * *******************************************************************/
static u32_t ti_diff(u32_t old_counter, u32_t counter)
{
  static const u32_t MAX_COUNTER = ~0;
  u32_t diff;
  if( counter >= old_counter){
    diff = counter - old_counter;
  } else { //Case when the counter returns to zero (overflows).
    diff = counter +1 + (MAX_COUNTER - old_counter);
  }
  return diff;
}

/*!
 * Function name: ti_microseconds_to_ticks
 * \return the number of tick to achieve the time.
 * \param time: [in] time in microseconds
 * \brief Given a time, it returns the number of ticks
 * *******************************************************************/
u32_t ti_microseconds_to_ticks(u32_t time)
{
  static const u32_t MAX_COUNTER = (~0);
  static const u32_t CPU_NORMALIZED = XPAR_CPU_CORE_CLOCK_FREQ_HZ/100000;// tranform 87500000HZ to 875
  u32_t tick_number;

  if( time <= (MAX_COUNTER / CPU_NORMALIZED)){//lesser than 49 microseconds
    tick_number = (time * CPU_NORMALIZED)/10;
  } else {
    tick_number = (time/2) * (CPU_NORMALIZED/5); //valid for (time <= 49s) (Split the previous 10 into 2*5)   
  }
  return tick_number;
}
