// read/ write DDR buffer internal command
#pragma once
#include "ap_int.h"

/***
 * @brief DataMoverCmd
 * Refenrence:
 * https://www.xilinx.com/support/documentation/ip_documentation/axi_datamover/v5_1/pg022_axi_datamover.pdf
 * Table 2‐7: Command Word Description
 */

/**
 *  assign m_axis_mm2s_cmd_tvalid = s_axis_req_tvalid & s_axis_req_tready;//???
    assign m_axis_mm2s_cmd_tdata[22:0] = cur_req_len; // Bytes to transfer
    assign m_axis_mm2s_cmd_tdata[23] = 1'b1; // Type (INCR)
    assign m_axis_mm2s_cmd_tdata[29:24] = 0; // DRE Stream alignment, not used
    assign m_axis_mm2s_cmd_tdata[30] = 1'b1; // End of Frame
    assign m_axis_mm2s_cmd_tdata[31] = 1'b0; // DRE ReAlignment Request, not used
    assign m_axis_mm2s_cmd_tdata[71:32] = cur_req_addr; // Start address
    assign m_axis_mm2s_cmd_tdata[79:72] = 0; // Reserved 

*/

struct DataMoverCmd {
#define DATAMOVER_BBT_BITS 23
  ap_uint<DATAMOVER_BBT_BITS> bbt;   /// Bytes to Transfer, 23-bits field, from 1 to 8388607 Bytes
  ap_uint<1>                  type;  /// 1 enables INCR, 0 enables FIXED addr AXI4 trans
  ap_uint<6>                  dsa;   /// DRE Stream Alignment, not used
  ap_uint<1>  eof;    /// End of Frame, when it is set, MM2S assert TLAST on the last Data
  ap_uint<1>  drr;    /// DRE ReAlignment Request
  ap_uint<40> saddr;  /// Start Address
  ap_uint<4>  tag;
  ap_uint<4>  rsvd;
  DataMoverCmd() : bbt(0), type(0), dsa(0), eof(0), drr(0), saddr(0), tag(0), rsvd(0) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "BTT: " << bbt.to_string(16) << "\t";
    sstream << "Addr: " << saddr.to_string(16) << "\t";
    return sstream.str();
  }
#endif
};

struct DataMoverStatus {
  ap_uint<4> tag;
  ap_uint<1> interr;
  ap_uint<1> decerr;
  ap_uint<1> slverr;
  ap_uint<1> okay;
  DataMoverStatus() : tag(0), interr(0), decerr(0), slverr(0), okay(0) {}
#ifndef __SYNTHESIS__
  std::string to_string() {
    std::stringstream sstream;
    sstream << "Tag: " << tag.to_string(16) << "\t";
    sstream << "Okay: " << okay.to_string(16) << "\t";
    return sstream.str();
  }
#else
  INLINE char *to_string() { return 0; }
#endif
};


