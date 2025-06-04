#include "ap_int.h"
#include "constexpr_math.hpp"
#include "hlsacc_types.hpp"

// #pragma GCC diagnostic push
// #pragma GCC diagnostic error "-Wpedantic"
// #pragma GCC diagnostic error "-Wall"
// #pragma GCC diagnostic error "-Wextra"
// #pragma GCC diagnostic ignored "-Wunused-label"

template <typename DATA_TYPE, unsigned int MAX_ACC_FIFO_SIZE>
class accFIFO
{
protected:
    static constexpr unsigned int POINTER_LENGTH = (unsigned int)constexpr_math::log2_ceil(MAX_ACC_FIFO_SIZE);
    static_assert(MAX_ACC_FIFO_SIZE > 0 && MAX_ACC_FIFO_SIZE <= (40960));
    DATA_TYPE _buf[MAX_ACC_FIFO_SIZE];
    // acc_data_pkt _buf[MAX_ACC_FIFO_SIZE];
    ap_uint<POINTER_LENGTH> _tailer = 0;
    ap_uint<POINTER_LENGTH> _header = 0;
    ap_uint<POINTER_LENGTH + 1> _length = 0;

public:
    accFIFO()
    {
#pragma HLS bind_storage variable = _buf type = RAM_2P impl = BRAM
        _tailer = 0;
        _header = 0;
        _length = 0;
    }
    int head(DATA_TYPE &channel_1)
    {
#pragma HLS inline
//#pragma HLS DEPENDENCE false variable = _buf
        if (_length != ap_uint<POINTER_LENGTH + 1>(0))
        {
            channel_1 = _buf[_header];
            return 0;
        }
        else
            return 1;
    }
    int pop()
    {
#pragma HLS inline
        if (_length != ap_uint<POINTER_LENGTH + 1>(0))
        {
            _header++;
            _length--;
            return 0;
        }
        else
            return 1;
    }
    int push(DATA_TYPE &channel_1)
    {
#pragma HLS inline
        // #pragma HLS DEPENDENCE false variable = _buf
        if (_length != ap_uint<POINTER_LENGTH + 1>(MAX_ACC_FIFO_SIZE))
        {
            _buf[_tailer] = channel_1;
            _tailer++;
            _length++;
            return 0;
        }
        else
            return 1;
    }
    unsigned int size()
    {
        return _length;
    }
    unsigned int capacity()
    {
        return MAX_ACC_FIFO_SIZE;
    }
    bool full()
    {
        return _length == ap_uint<POINTER_LENGTH + 1>(MAX_ACC_FIFO_SIZE);
    }
    bool empty()
    {
        return _length == ap_uint<POINTER_LENGTH + 1>(0);
    }
};
/// @brief 有两套指针的FIFO
/// 第一套是假指针，从FIFO使用假指针读数据并POP，实际FIFO当前保存数据大小不会变化
/// 第二套是真指针，从FIFO使用真指针读写数据并POP，FIFO才会实际推出数据
/// 真指针和假指针都针对head指针而论，两套公用一个tail指针
/// @tparam DATA_TYPE
/// @tparam MAX_ACC_FIFO_SIZE
template <typename DATA_TYPE, unsigned int MAX_ACC_FIFO_SIZE>
class accDualPointerFIFO : public accFIFO<DATA_TYPE, MAX_ACC_FIFO_SIZE>
{
protected:
    static constexpr unsigned int POINTER_LENGTH = (unsigned int)constexpr_math::log2_ceil(MAX_ACC_FIFO_SIZE);
    static_assert(MAX_ACC_FIFO_SIZE > 0 && MAX_ACC_FIFO_SIZE <= (40960));
    ap_uint<POINTER_LENGTH> _fake_header = 0;
    ap_uint<POINTER_LENGTH> _fake_length = 0;

public:
    int fake_head(DATA_TYPE &channel_1)
    {
#pragma HLS inline
        if (_fake_length != ap_uint<POINTER_LENGTH + 1>(0) && this->_length != ap_uint<POINTER_LENGTH + 1>(0))
        {
            channel_1 = this->_buf[_fake_header];
            return 0;
        }
        else
            return 1;
    }
    int fake_pop()
    {
#pragma HLS inline
        if (_fake_length != ap_uint<POINTER_LENGTH + 1>(0) && this->_length != ap_uint<POINTER_LENGTH + 1>(0))
        {
            _fake_header++;
            _fake_length--;
            return 0;
        }
        else
            return 1;
    }
    int fake_push(DATA_TYPE &channel_1)
    {
#pragma HLS inline
        // #pragma HLS DEPENDENCE false variable = _buf
        if (this->_length != ap_uint<POINTER_LENGTH + 1>(MAX_ACC_FIFO_SIZE))
        {
            this->_buf[this->_tailer] = channel_1;
            this->_tailer++;
            this->_length++;
            _fake_length++;
            return 0;
        }
        else
            return 1;
    }
    unsigned int fake_size()
    {
#pragma HLS inline
        return _fake_length;
    }
    unsigned int capacity()
    {
#pragma HLS inline
        return MAX_ACC_FIFO_SIZE;
    }
    bool fake_full()
    {
#pragma HLS inline
        return this->full();
    }
    bool fake_empty()
    {
#pragma HLS inline
        return _fake_length == ap_uint<POINTER_LENGTH + 1>(0) || this->_length == ap_uint<POINTER_LENGTH + 1>(0);
    }
    int push(DATA_TYPE &channel_1)
    {
#pragma HLS inline
        if (this->accFIFO<DATA_TYPE, MAX_ACC_FIFO_SIZE>::push(channel_1) == 0)
        {
            _fake_length++;
            return 0;
        }
        else
        {
            return 1;
        }
    }
    int pop()
    {
#pragma HLS inline
        // 如果真长度和假长度一样，那么代表修改就需要同步
        // 要保证假长度始终小于或者等于真长度
        ap_uint<POINTER_LENGTH> _temp_length = this->_length;
        if (this->accFIFO<DATA_TYPE, MAX_ACC_FIFO_SIZE>::pop() == 0)
        {
            if (_temp_length == _fake_length)
            {
                _fake_length--;
                _fake_header++;
            }
            return 0;
        }
        else
        {
            return 1;
        }
    }
    void pointer_sync(){
        _fake_header = this->_header;
        _fake_length = this->_length;
    }
    void clear()
    {
#pragma HLS inline
        _fake_header = 0;
        _fake_length = 0;
        this->_length = 0;
        this->_header = 0;
        this->_tailer = 0;
    }
};
// #pragma GCC diagnostic pop
