// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
/*
#-  (c) Copyright 2011-2020 Xilinx, Inc. All rights reserved.
#-
#-  This file contains confidential and proprietary information
#-  of Xilinx, Inc. and is protected under U.S. and
#-  international copyright and other intellectual property
#-  laws.
#-
#-  DISCLAIMER
#-  This disclaimer is not a license and does not grant any
#-  rights to the materials distributed herewith. Except as
#-  otherwise provided in a valid license issued to you by
#-  Xilinx, and to the maximum extent permitted by applicable
#-  law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
#-  WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
#-  AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
#-  BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
#-  INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
#-  (2) Xilinx shall not be liable (whether in contract or tort,
#-  including negligence, or under any other theory of
#-  liability) for any loss or damage of any kind or nature
#-  related to, arising under or in connection with these
#-  materials, including for any direct, or any indirect,
#-  special, incidental, or consequential loss or damage
#-  (including loss of data, profits, goodwill, or any type of
#-  loss or damage suffered as a result of any action brought
#-  by a third party) even if such damage or loss was
#-  reasonably foreseeable or Xilinx had been advised of the
#-  possibility of the same.
#-
#-  CRITICAL APPLICATIONS
#-  Xilinx products are not designed or intended to be fail-
#-  safe, or for use in any application requiring fail-safe
#-  performance, such as life-support or safety devices or
#-  systems, Class III medical devices, nuclear facilities,
#-  applications related to the deployment of airbags, or any
#-  other applications that could lead to death, personal
#-  injury, or severe property or environmental damage
#-  (individually and collectively, "Critical
#-  Applications"). Customer assumes the sole risk and
#-  liability of any use of Xilinx products in Critical
#-  Applications, subject only to applicable laws and
#-  regulations governing limitations on product liability.
#-
#-  THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
#-  PART OF THIS FILE AT ALL TIMES. 
#- ************************************************************************


   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef HLSACC_STREAM
#define HLSACC_STREAM



/*
 * This file contains a C++ model of hls::stream.
 * It defines C simulation model.
 */
#ifndef __cplusplus

#error C++ is required to include this header file

#else

//////////////////////////////////////////////
// C++ Software model for hls::stream
//////////////////////////////////////////////
#include <queue>
#include <iostream>
#include <typeinfo>
#include <hls_stream.h>
#include <ap_axi_sdata.h>
#include <ap_int.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <cstring>

#ifndef _MSC_VER
#include <cxxabi.h>
#include <stdlib.h>
#endif

namespace hlsacc {

static int instance_count;
static int max_size;
static void stream_handler() {
  //std::cout << "The maximum depth reached by any of the " << instance_count 
  //  << " hls::stream() instances in the design is " << max_size << std::endl;
}

template<size_t SIZE>
class stream_map {
  protected:
    //static std::unordered_map<void*, std::deque<std::array<char, SIZE>>>& deque_map();
    static std::unordered_map<void*, std::string>& name_map();
};


template<size_t SIZE>
std::unordered_map<void*, std::string>& stream_map<SIZE>::name_map() {
  static auto* name_map = new std::unordered_map<void*, std::string>();
  return *name_map;
};

template <size_t WData, size_t WUser, size_t WId, size_t WDest>
class stream : public stream_map<sizeof(hls::axis<ap_int<WData>, WUser, WId, WDest>)>
{
  using BaseType = stream_map<sizeof(hls::axis<ap_int<WData>, WUser, WId, WDest>)>;
  using DataType = hls::axis<ap_int<WData>, WUser, WId, WDest>;
  
  protected:
    DataType _data;
    void* buffer;
    uint64_t capacity;
    uint64_t head;
    uint64_t tail;
    uint64_t buffer_size;
    int mode; // 0: internally allocated memory, 1: externally provided memory
    
    
  protected:
    void register_handler() {
      if (instance_count == 0) {
        const int handler = std::atexit(stream_handler);
      }
      instance_count++;
    }
    
    using BaseType::name_map;

  public:
    /// Constructors
    // Keep consistent with the synthesis model's constructors
    stream() {
      uint64_t cap = 4096/(WData/8);
      static unsigned _counter = 1;
      register_handler();
      std::stringstream ss;
      name_map().erase(&_data);
      this->buffer = malloc(cap * sizeof(DataType));
      this->buffer_size = 0;
      this->head = this->tail = 0;
      this->capacity = cap;
      this->mode = 0; // Internally allocated memory
#ifndef _MSC_VER
      char* _demangle_name = abi::__cxa_demangle(typeid(*this).name(), 0, 0, 0);
      if (_demangle_name) {
          name_map()[&_data] = _demangle_name;
          free(_demangle_name);
      }
      else {
          name_map()[&_data] = "hls_stream";
      }
#else
      name_map()[&_data] = typeid(*this).name();
#endif

      ss << _counter++;
      name_map()[&_data] += "." + ss.str();
  }

    stream(uint64_t cap) {
        static unsigned _counter = 1;
        register_handler();
        std::stringstream ss;
        name_map().erase(&_data);
        this->buffer = malloc(cap * sizeof(DataType));
        this->buffer_size = 0;
        this->head = this->tail = 0;
        this->capacity = cap;
        this->mode = 0; // Internally allocated memory
#ifndef _MSC_VER
        char* _demangle_name = abi::__cxa_demangle(typeid(*this).name(), 0, 0, 0);
        if (_demangle_name) {
            name_map()[&_data] = _demangle_name;
            free(_demangle_name);
        }
        else {
            name_map()[&_data] = "hls_stream";
        }
#else
        name_map()[&_data] = typeid(*this).name();
#endif

        ss << _counter++;
        name_map()[&_data] += "." + ss.str();
    }

    stream(const char* name, uint64_t cap) {
        // default constructor, capacity set to predefined maximum
        name_map()[&_data] = name;
        register_handler();
        this->capacity = cap;
        this->buffer = malloc(cap * sizeof(DataType));
        this->buffer_size = 0;
        this->head = this->tail = 0;
        this->mode = 0; // Internally allocated memory
    }

    stream(const char* name) {
      uint64_t cap = 4096/(WData/8);
      // default constructor, capacity set to predefined maximum
      name_map()[&_data] = name;
      register_handler();
      this->capacity = cap;
      this->buffer = malloc(cap * sizeof(DataType));
      this->buffer_size = 0;
      this->head = this->tail = 0;
      this->mode = 0; // Internally allocated memory
    }

    stream(const char* name, void* addr, uint64_t cap, uint64_t initial_size) {
        name_map()[&_data] = name;
        register_handler();
        this->buffer = addr;
        this->capacity = cap;
        this->head = 0;
        this->buffer_size = initial_size;
        this->tail = initial_size % cap;
        this->mode = 1; // Externally provided memory
        this->__recv_done_signal = !(initial_size==0);
    }

    /// Destructor
    ~stream() {
        /*
        if (this->buffer_size != 0) {
            std::cout << "WARNING: Hls::stream '" 
                     << name_map()[&_data]
                     << "' contains leftover data,"
                     << " which may result in RTL simulation hanging."
                     << std::endl;
        }*/
        
        // Only free memory if it was allocated internally (mode == 0)
        if (mode == 0 && buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }

  /// Make copy constructor and assignment operator private
  private:
    stream(const stream<WData,WUser, WId,WDest>& chn) {
        name_map()[&_data] = name_map()[&chn._data];
        buffer = chn.buffer;
        head = chn.head;
        tail = chn.tail;
        buffer_size = chn.buffer_size;
        capacity = chn.capacity;
        mode = chn.mode;
    }

    stream& operator = (const stream<WData,WUser, WId,WDest>& chn) {
        name_map()[&_data] = name_map()[&chn._data];
        buffer = chn.buffer;
        head = chn.head;
        tail = chn.tail;
        buffer_size = chn.buffer_size;
        capacity = chn.capacity;
        mode = chn.mode;
        return *this;
    }

  private:
    bool __send_done_signal = false;
    bool __recv_done_signal = false;
    bool __empty() const {
        return this->buffer_size == 0;
    }    
  public:
    /// Overload >> and << operators to implement read() and write()
    void operator >> (DataType& rdata) {
        read(rdata);
    }

    void operator << (const DataType& wdata) {
        write(wdata);
    }

    /// Status of the queue
    bool empty() const {
        bool res =  __empty()&&!(((this->__recv_done_signal))&&!(this->__send_done_signal));
        //printf("RES%d recv_done_signal%d send_done_signal%d\n",res,this->__recv_done_signal,this->__send_done_signal);
        return res;
    }    

    

    bool full() const { 
        return this->capacity == this->buffer_size;
    }
    
    bool exist() {
        return true;
    }

    /// Blocking read
    void read(DataType& head_data) {
        head_data = read();
    }

    /// Blocking read with dependency
    bool read_dep(DataType& head_data, volatile bool flag) {
        head_data = read();
        return flag;
    }

    DataType read() {
        DataType elem;
        if (__empty()) {
            elem.data = 0;
            elem.last = this->__recv_done_signal;
            elem.keep = static_cast<ap_int<WData/8>>(-1);  // All bits set
            elem.user = this->__recv_done_signal?0xff:0; 
            __send_done_signal = this->__recv_done_signal;
            //printf("RETURN FF\n");
        } else {
            elem.data = ((ap_uint<WData>*)(this->buffer))[this->head];
            elem.last = (this->buffer_size==1);
            elem.keep = static_cast<ap_int<WData/8>>(-1);  // All bits set
            elem.user = 0;
            this->head = (this->head + 1) % this->capacity;
            --this->buffer_size;
            //printf("BUFFER SIZE%d\n",this->buffer_size);
        }
        return elem;
    }

    /// Blocking write
    void write(const DataType& tail_data) { 
        if (!full()) {  
          ((ap_uint<WData>*)(this->buffer))[this->tail] = tail_data.data;
          this->tail = (this->tail + 1) % this->capacity;
          this->buffer_size++;
          if(tail_data.user == 0xff||tail_data.user == 0xf0){
            this->__recv_done_signal = true;
          }
      } else {
          std::cerr << "Stream is full!" << std::endl;
      }
  }

  /// Blocking write with dependency
  bool write_dep(const DataType& tail_data, volatile bool flag) { 
      write(tail_data);
      return flag;
  }

  /// Nonblocking read
  bool read_nb(DataType& head_data) {
      bool is_empty = empty();
      if (is_empty) {
          head_data = DataType();
      } else {
          head_data = read();
      }
      return !is_empty;
  }

  /// Nonblocking write
  bool write_nb(const DataType& tail_data) {
      bool is_full = full();
      if (!is_full) {
          write(tail_data);
      }
      return !is_full;
  }

  /// Fifo size
  size_t size() const {
      return this->buffer_size;
  }

  bool last(){
    return this->__recv_done_signal;
  }

  void reset(void* addr, uint64_t cap, uint64_t initial_size) {
    if(this->mode==0&&this->buffer!=NULL){
        free(this->buffer);
    }
    this->buffer = addr;
    this->capacity = cap;
    this->head = 0;
    this->buffer_size = initial_size;
    this->tail = initial_size % cap;
    this->mode = 1; // Externally provided memory
    this->__send_done_signal = false;
    this->__recv_done_signal = !(initial_size==0);
  }
};


} // namespace hlsacc

#endif // __cplusplus
#endif  

