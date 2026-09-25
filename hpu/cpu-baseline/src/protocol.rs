use bytemuck::{cast_slice, cast_slice_mut};
use std::io::{Read, Write};
use std::time::{Duration, Instant};

pub const FRAME_REQUEST: u64 = 1;
pub const FRAME_RESPONSE: u64 = 2;
pub const FRAME_ERROR: u64 = 3;
pub const OP_ADD_SCALAR_U8_HPU_NATIVE_ROUNDTRIP: u64 = 3;
pub const OP_ADD_U8: u64 = 0x100;
pub const OP_SUB_U8: u64 = 0x101;
pub const OP_MUL_U8: u64 = 0x102;
pub const OP_DIV_U8: u64 = 0x103;
pub const OP_REM_U8: u64 = 0x104;
pub const OP_BITAND_U8: u64 = 0x110;
pub const OP_BITOR_U8: u64 = 0x111;
pub const OP_BITXOR_U8: u64 = 0x112;
pub const OP_BITNOT_U8: u64 = 0x113;
pub const OP_SHL_U8: u64 = 0x120;
pub const OP_SHR_U8: u64 = 0x121;
pub const OP_ROTL_U8: u64 = 0x122;
pub const OP_ROTR_U8: u64 = 0x123;
pub const OP_EQ_U8: u64 = 0x130;
pub const OP_NE_U8: u64 = 0x131;
pub const OP_LT_U8: u64 = 0x132;
pub const OP_LE_U8: u64 = 0x133;
pub const OP_GT_U8: u64 = 0x134;
pub const OP_GE_U8: u64 = 0x135;
pub const OP_SUB_SCALAR_U8: u64 = 0x200;
pub const OP_RSUB_SCALAR_U8: u64 = 0x201;
pub const OP_MUL_SCALAR_U8: u64 = 0x202;
pub const OP_DIV_SCALAR_U8: u64 = 0x203;
pub const OP_REM_SCALAR_U8: u64 = 0x204;
pub const OP_SHL_SCALAR_U8: u64 = 0x210;
pub const OP_SHR_SCALAR_U8: u64 = 0x211;
pub const OP_ROTL_SCALAR_U8: u64 = 0x212;
pub const OP_ROTR_SCALAR_U8: u64 = 0x213;
pub const OP_INPUT_HPU_NATIVE: u64 = 1 << 62;
pub const OP_OUTPUT_HPU_NATIVE: u64 = 1 << 61;
pub const VERSION_TIMING: u64 = 2;

pub fn operation_operand_count(operation: u64) -> usize {
    match operation & !(OP_INPUT_HPU_NATIVE | OP_OUTPUT_HPU_NATIVE) {
        OP_ADD_U8 | OP_SUB_U8 | OP_MUL_U8 | OP_DIV_U8 | OP_REM_U8 | OP_BITAND_U8 | OP_BITOR_U8
        | OP_BITXOR_U8 | OP_SHL_U8 | OP_SHR_U8 | OP_ROTL_U8 | OP_ROTR_U8 | OP_EQ_U8 | OP_NE_U8
        | OP_LT_U8 | OP_LE_U8 | OP_GT_U8 | OP_GE_U8 => 2,
        _ => 1,
    }
}

const MAGIC: &[u8; 8] = b"LWERPC01";
const HEADER_U64S: usize = 14;
const TIMING_MAGIC: &[u8; 8] = b"LWEBEN01";
const TIMING_VERSION: u64 = 1;
const TIMING_U64S: usize = 15;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BatchMetadata {
    pub mask_dimension: usize,
    pub item_count: usize,
    pub radix_blocks_per_item: usize,
    pub message_width: usize,
    pub carry_width: usize,
    pub padding_bit_width: usize,
    pub delta_log2: usize,
    pub ciphertext_word_count: usize,
}

impl BatchMetadata {
    pub fn expected_hpu_native_word_count(&self) -> Result<usize, String> {
        const HPU_NATIVE_WORDS_PER_LWE: usize = 2 * 1536;
        self.item_count
            .checked_mul(self.radix_blocks_per_item)
            .and_then(|count| count.checked_mul(HPU_NATIVE_WORDS_PER_LWE))
            .ok_or_else(|| "HPU-native ciphertext word count overflow".to_string())
    }

    pub fn validate_hpu_native(&self) -> Result<(), String> {
        if self.item_count == 0 || self.mask_dimension == 0 || self.radix_blocks_per_item == 0 {
            return Err("invalid zero-sized ciphertext shape".to_string());
        }
        let expected = self.expected_hpu_native_word_count()?;
        if self.ciphertext_word_count != expected {
            return Err(format!(
                "HPU-native word count mismatch: metadata={}, expected={expected}",
                self.ciphertext_word_count
            ));
        }
        Ok(())
    }
}

#[derive(Debug)]
pub struct CiphertextFrame {
    pub version: u64,
    pub kind: u64,
    pub request_id: u64,
    pub operation: u64,
    pub status: u64,
    pub scalar: u64,
    pub metadata: BatchMetadata,
    pub ciphertext_words: Vec<u64>,
    pub error_message: Option<String>,
}

#[derive(Clone, Debug, Default)]
pub struct ServerTiming {
    pub request_id: u64,
    pub request_receive_ns: u64,
    pub request_validate_ns: u64,
    pub request_decode_ns: u64,
    pub hpu_prepare_ns: u64,
    pub hpu_enqueue_ns: u64,
    pub hpu_wait_sync_ns: u64,
    pub hpu_output_convert_ns: u64,
    pub result_encode_ns: u64,
    pub mem_sanitizer_ns: u64,
    pub remote_process_ns: u64,
    pub response_send_ns: u64,
    pub server_total_ns: u64,
    pub response_payload_bytes: u64,
}

#[derive(Clone, Debug, Default)]
pub struct ReceiveTiming {
    pub response_header: Duration,
    pub response_payload: Duration,
    pub telemetry: Duration,
}

pub fn write_request(
    stream: &mut impl Write,
    request_id: u64,
    operation: u64,
    scalar: u8,
    metadata: &BatchMetadata,
    words: &[u64],
) -> Result<(), String> {
    metadata.validate_hpu_native()?;
    let expected_words = metadata
        .ciphertext_word_count
        .checked_mul(operation_operand_count(operation))
        .ok_or_else(|| "request payload word count overflow".to_string())?;
    if words.len() != expected_words {
        return Err(format!(
            "request payload word count mismatch: payload={}, expected={expected_words}",
            words.len()
        ));
    }
    let payload_bytes = words
        .len()
        .checked_mul(size_of::<u64>())
        .ok_or_else(|| "request payload size overflow".to_string())?;
    let fields = [
        VERSION_TIMING,
        FRAME_REQUEST,
        request_id,
        operation,
        0,
        u64::from(scalar),
        as_u64(metadata.mask_dimension, "mask_dimension")?,
        as_u64(metadata.item_count, "item_count")?,
        as_u64(metadata.radix_blocks_per_item, "radix_blocks_per_item")?,
        as_u64(metadata.message_width, "message_width")?,
        as_u64(metadata.carry_width, "carry_width")?,
        as_u64(metadata.padding_bit_width, "padding_bit_width")?,
        as_u64(metadata.delta_log2, "delta_log2")?,
        as_u64(metadata.ciphertext_word_count, "ciphertext_word_count")?,
    ];

    let mut header = Vec::with_capacity(8 + (HEADER_U64S + 1) * 8);
    header.extend_from_slice(MAGIC);
    for field in fields {
        header.extend_from_slice(&field.to_le_bytes());
    }
    header.extend_from_slice(&as_u64(payload_bytes, "payload_bytes")?.to_le_bytes());
    stream
        .write_all(&header)
        .map_err(|err| format!("unable to send request header: {err}"))?;
    stream
        .write_all(cast_slice(words))
        .map_err(|err| format!("unable to send request payload: {err}"))?;
    stream
        .flush()
        .map_err(|err| format!("unable to flush request: {err}"))
}

pub fn read_response_and_timing(
    stream: &mut impl Read,
    max_payload_bytes: usize,
) -> Result<(CiphertextFrame, ServerTiming, ReceiveTiming), String> {
    let mut receive_timing = ReceiveTiming::default();
    let header_start = Instant::now();
    let mut magic = [0_u8; 8];
    stream
        .read_exact(&mut magic)
        .map_err(|err| format!("unable to read response magic: {err}"))?;
    if &magic != MAGIC {
        return Err(format!("invalid response magic: {magic:02x?}"));
    }
    let mut fields = [0_u64; HEADER_U64S];
    for field in &mut fields {
        *field = read_u64(stream)?;
    }
    let payload_bytes = as_usize(read_u64(stream)?, "payload_bytes")?;
    receive_timing.response_header = header_start.elapsed();
    if payload_bytes > max_payload_bytes {
        return Err(format!(
            "response payload is too large: {payload_bytes}, limit={max_payload_bytes}"
        ));
    }

    let metadata = BatchMetadata {
        mask_dimension: as_usize(fields[6], "mask_dimension")?,
        item_count: as_usize(fields[7], "item_count")?,
        radix_blocks_per_item: as_usize(fields[8], "radix_blocks_per_item")?,
        message_width: as_usize(fields[9], "message_width")?,
        carry_width: as_usize(fields[10], "carry_width")?,
        padding_bit_width: as_usize(fields[11], "padding_bit_width")?,
        delta_log2: as_usize(fields[12], "delta_log2")?,
        ciphertext_word_count: as_usize(fields[13], "ciphertext_word_count")?,
    };

    let payload_start = Instant::now();
    if fields[1] == FRAME_ERROR {
        let mut payload = vec![0_u8; payload_bytes];
        stream
            .read_exact(&mut payload)
            .map_err(|err| format!("unable to read error payload: {err}"))?;
        let message = String::from_utf8(payload)
            .map_err(|err| format!("remote error is not UTF-8: {err}"))?;
        return Err(format!("remote HPU request failed: {message}"));
    }
    if payload_bytes % size_of::<u64>() != 0 {
        return Err(format!(
            "response payload is not u64-aligned: {payload_bytes}"
        ));
    }
    metadata.validate_hpu_native()?;
    if payload_bytes != metadata.ciphertext_word_count * size_of::<u64>() {
        return Err("response payload length does not match metadata".to_string());
    }
    let mut words = vec![0_u64; metadata.ciphertext_word_count];
    stream
        .read_exact(cast_slice_mut(&mut words))
        .map_err(|err| format!("unable to read response payload: {err}"))?;
    receive_timing.response_payload = payload_start.elapsed();

    let frame = CiphertextFrame {
        version: fields[0],
        kind: fields[1],
        request_id: fields[2],
        operation: fields[3],
        status: fields[4],
        scalar: fields[5],
        metadata,
        ciphertext_words: words,
        error_message: None,
    };

    let telemetry_start = Instant::now();
    let timing = read_server_timing(stream)?;
    receive_timing.telemetry = telemetry_start.elapsed();
    Ok((frame, timing, receive_timing))
}

fn read_server_timing(stream: &mut impl Read) -> Result<ServerTiming, String> {
    let mut magic = [0_u8; 8];
    stream
        .read_exact(&mut magic)
        .map_err(|err| format!("unable to read timing magic: {err}"))?;
    if &magic != TIMING_MAGIC {
        return Err(format!("invalid timing magic: {magic:02x?}"));
    }
    let mut fields = [0_u64; TIMING_U64S];
    for field in &mut fields {
        *field = read_u64(stream)?;
    }
    if fields[0] != TIMING_VERSION {
        return Err(format!("unsupported timing version {}", fields[0]));
    }
    Ok(ServerTiming {
        request_id: fields[1],
        request_receive_ns: fields[2],
        request_validate_ns: fields[3],
        request_decode_ns: fields[4],
        hpu_prepare_ns: fields[5],
        hpu_enqueue_ns: fields[6],
        hpu_wait_sync_ns: fields[7],
        hpu_output_convert_ns: fields[8],
        result_encode_ns: fields[9],
        mem_sanitizer_ns: fields[10],
        remote_process_ns: fields[11],
        response_send_ns: fields[12],
        server_total_ns: fields[13],
        response_payload_bytes: fields[14],
    })
}

fn read_u64(stream: &mut impl Read) -> Result<u64, String> {
    let mut bytes = [0_u8; 8];
    stream
        .read_exact(&mut bytes)
        .map_err(|err| format!("unable to read protocol u64: {err}"))?;
    Ok(u64::from_le_bytes(bytes))
}

fn as_u64(value: usize, label: &str) -> Result<u64, String> {
    u64::try_from(value).map_err(|_| format!("{label} does not fit in u64"))
}

fn as_usize(value: u64, label: &str) -> Result<usize, String> {
    usize::try_from(value).map_err(|_| format!("{label} does not fit in usize"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn native_metadata_size_is_stable() {
        let metadata = BatchMetadata {
            mask_dimension: 2048,
            item_count: 128,
            radix_blocks_per_item: 4,
            message_width: 2,
            carry_width: 2,
            padding_bit_width: 1,
            delta_log2: 59,
            ciphertext_word_count: 128 * 4 * 2 * 1536,
        };
        metadata.validate_hpu_native().unwrap();
        assert_eq!(metadata.ciphertext_word_count * 8, 12_582_912);
    }
}
