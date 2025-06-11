# SUDA Computational Storage API Quick Reference Guide
SUDA is built on APIs that comply with the NVMe CS standard. Based on these APIs, SNIA CS APIs are encapsulated. For SNIA CS APIs, please refer to the SNIA CS API documentation. This document only introduces the underlying APIs.

## Memory Range Set Management Functions

### nvme_operate_memory_range_set
**Function**: Perform operations on memory range sets (create/delete, etc.)
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `op`: Operation type
- `numr`: Number of memory ranges
- `rsid`: Result ID (input/output parameter)
- `mmrange_descri`: Memory range descriptor

### nvme_create_memory_range_set
**Function**: Create a memory range set
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `rsid`: Range set ID (output parameter)
- `numr`: Number of memory ranges
- `mmrange_descri`: Memory range descriptor

### nvme_delete_memory_range_set
**Function**: Delete a memory range set
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `rsid`: Range set ID to delete

## Program Management Functions

### nvme_operate_program
**Function**: Perform operations on programs (load/unload, etc.)
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `pind`: Program index
- `ptype`: Program type
- `psize`: Program size
- `numb`: Number of bytes to transfer
- `load_offset`: Load offset
- `pit`: Program ID type
- `sel`: Selector
- `program_data`: Program data
- `pid`: Program ID

### nvme_load_hlsacc_program
**Function**: Load an HLS accelerator program
**Parameters**:
- `fd`: Device file descriptor
- `psize`: Program size
- `pind`: Program index
- `nsid`: Computational namespace ID
- `program_data`: Program data

### nvme_unload_hlsacc_program
**Function**: Unload an HLS accelerator program
**Parameters**:
- `fd`: Device file descriptor
- `pind`: Program index
- `nsid`: Computational namespace ID

### nvme_program_mgmt
**Function**: Program management (activate/deactivate, etc.)
**Parameters**:
- `fd`: Device file descriptor
- `pind`: Program index
- `sel`: Selector (operation type)
- `nsid`: Computational namespace ID

### nvme_activate_program
**Function**: Activate a program
**Parameters**:
- `fd`: Device file descriptor
- `pind`: Program index
- `nsid`: Computational namespace ID

### nvme_deactivate_program
**Function**: Deactivate a program
**Parameters**:
- `fd`: Device file descriptor
- `pind`: Program index
- `nsid`: Computational namespace ID

## Program Execution Functions

### nvme_execute_program
**Function**: Execute a loaded program
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `rsid`: Memory range set ID
- `pind`: Program index
- `numr`: Number of memory ranges
- `dlen`: Data length
- `data_buffer`: Data buffer
- `capram1`: Capture parameter 1
- `capram2`: Capture parameter 2
- `capram3`: Capture parameter 3
- `result`: Execution result

### nvme_execute_hlsacc_program
**Function**: Execute an HLS accelerator program
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `rsid`: Memory range set ID
- `pind`: Program index
- `contexts`: Accelerator context array
- `context_num`: Number of contexts
- `numr`: Number of memory ranges
- `priority`: Priority
- `result`: Execution result

### nvme_execute_hlsacc_programV2
**Function**: Execute an HLS accelerator program (enhanced version)
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Computational namespace ID
- `rsid`: Memory range set ID
- `pind`: Program index
- `contexts`: Accelerator context array
- `context_num`: Number of contexts
- `numr`: Number of memory ranges
- `priority`: Priority
- `result`: Execution result
- `run_way`: Execution mode

## Storage Layer Memory Operation Functions

### nvme_slm_read
**Function**: Read data from a specified namespace
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Namespace ID
- `starting_bytes`: Starting byte position for reading
- `read_length`: Read length (bytes)
- `data`: Data storage location

### nvme_slm_write
**Function**: Write data to a specified namespace
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Namespace ID
- `starting_bytes`: Starting byte position for writing
- `write_length`: Write length (bytes)
- `data`: Data source

### nvme_slm_copy
**Function**: Copy data between namespaces
**Parameters**:
- `fd`: Device file descriptor
- `source_range_entries`: Source range entry array
- `length`: Length
- `sdaddr`: Starting position of destination address
- `format_sel`: Descriptor format type (02h: LBA-based, 04h: byte-based)
- `nr`: Number of source ranges
- `nsid`: Destination namespace ID

### nvme_slm_fill
**Function**: Fill a memory range in the specified namespace with zeros
**Parameters**:
- `fd`: Device file descriptor
- `nsid`: Namespace ID
- `starting_bytes`: Starting byte position for filling
- `fill_length`: Fill length (bytes)


**Function**: Accelerator context structure
**Members**:
- `rubbish`: FIFO descriptor (24 long integers)
- `static_data`: Static variable data (2048 bytes)