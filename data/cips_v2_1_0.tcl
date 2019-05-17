###############################################################################
#
# Copyright (c) 2003 Xilinx, Inc.
# All rights reserved. 
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS".
# BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS ONE POSSIBLE 
# IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD, XILINX 
# IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE FROM 
# ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING 
# ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.  XILINX 
# EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO THE 
# ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY 
# WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE 
# FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY 
# AND FITNESS FOR A PARTICULAR PURPOSE.
#
# Author: Sathya Thammanur <Sathyanarayanan.Thammanur@xilinx.com>
# Author: Chris Borrelli <Chris.Borrelli@xilinx.com>
# Author: Jean-Marc David
#
# $Id: cips_v2_1_0.tcl,v 1.3 2012/09/11 15:04:29 jdavid Exp $
###############################################################################

#-----------
# Globals
#-----------

set emac_list [list]
set emaclite_list [list]
set temac_list [list]
set emac_mode ""
set dma ""
set emaclite_mode ""
set eth_intr_val ""
set emac_ver ""

#---------------------------------------------
# cips_drc - run drc checking (future work)
#---------------------------------------------
proc cips_drc {libhandle} {

    puts "Runnning DRC for the TCP-IP library... \n"
    
    set sw_proc_handle [xget_libgen_proc_handle]

}


proc cips_get_mode {inst_handle} {
    
    set mode "" 
    return $mode   
}

#------------------------------------
# generate - generates the .c file
#------------------------------------
proc generate {libhandle} {

   puts "Running generate for the TCP-IP library ..."
    
   puts "Generating application_options.h file ..."
   # Generate application_options.h file
   xgen_opts_file $libhandle
   
}



#------------------------------------------------------------------------------
# xgen_opts_file - Generates application_options.h file
#------------------------------------------------------------------------------
proc xgen_opts_file {libhandle} {

    set filename [file join "src/app_options" "application_options.h"] 
    file delete $filename
    set config_file [open $filename w]
    xprint_generated_header $config_file "Xilinx EDK cIPs User Settings"    
    puts $config_file ""
    

    # -----------------------------
    # Generate Network Adapter Options
    # -----------------------------
    puts $config_file "/* -------- Network Adapter Options ------- */"
    set recv_buf_size [xget_value $libhandle "PARAMETER" "recv_buf_size"]
    set network_mtu [xget_value $libhandle "PARAMETER" "network_mtu"]
    set max_udp [xget_value $libhandle "PARAMETER" "max_udp"]
    set max_tcp [xget_value $libhandle "PARAMETER" "max_tcp"]
    set max_tcp_seg [xget_value $libhandle "PARAMETER" "max_tcp_seg"]
    set max_net_adapter [xget_value $libhandle "PARAMETER" "max_net_adapter"]
    puts $config_file "\#define RECV_BUF_SIZE                   $recv_buf_size"
    puts $config_file "\#define NETWORK_MTU                     $network_mtu"
    puts $config_file "\#define MAX_UDP                         $max_udp"
    puts $config_file "\#define MAX_TCP                         $max_tcp"
    puts $config_file "\#define MAX_TCP_SEG                 $max_tcp_seg"
    puts $config_file "\#define MAX_NET_ADAPTER                 $max_net_adapter"
    puts $config_file ""


    puts $config_file "\#if \(NETWORK\_MTU \!\= 1518 \)"
    puts $config_file "\#error \"NETWORK\_MTU must equals 1518 on Xilinx.\""
    puts $config_file "\#endif"
    puts $config_file ""

    
    # ---------------------
    # Generate ARP options
    # ---------------------
    puts $config_file "/* ---------- ARP options ---------- */"
    set arp_table_size [xget_value $libhandle "PARAMETER" "arp_table_size"]
    puts $config_file "#define ARP_TABLE_SIZE                  $arp_table_size"

    puts $config_file ""
    

    # ---------------------------------
    # Generate debug
    # ---------------------------------

    puts $config_file "/* ---------- Debug options ---------- */"
    set debug_flag [xget_value $libhandle "PARAMETER" "debug_flag"]
    if { [string compare -nocase "true" $debug_flag] == 0} {
	puts $config_file "\#define T_DEBUG "
    } else {
	puts $config_file "\/\/\#define T_DEBUG "
    }
    puts $config_file ""


    # ---------------------------------
    # Generate debug
    # ---------------------------------

    puts $config_file "/* ---------- Xilinx printf ---------- */"
    puts $config_file "\#ifndef T_PLATFORM_DIAG"
    puts $config_file "\#define T_PLATFORM_DIAG(x) xil_printf x"
    puts $config_file "\#endif"
    puts $config_file ""

    # close config_file
    close $config_file
}

#------------------------------------------------------
# post_generate - doesn't do anything at the moment
#------------------------------------------------------
proc post_generate {libhandle} {
}

#-------------------------------------------------
# execs_generate
# This procedure builds the libcips.a
# library.
#-------------------------------------------------
proc execs_generate {libhandle} {
}


