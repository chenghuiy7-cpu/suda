#[cfg(not(all(target_arch = "x86_64", target_endian = "little")))]
compile_error!("suda-lwe-cpu-baseline currently supports little-endian x86_64 only");

use clap::{Parser, ValueEnum};
use rayon::prelude::*;
use std::fs::{self, File, OpenOptions};
use std::io::Write;
use std::net::{SocketAddr, TcpStream};
use std::os::unix::fs::FileExt;
use std::path::{Path, PathBuf};
use std::process::ExitCode;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use tfhe::core_crypto::prelude::LweCiphertextOwned;
use tfhe::integer::{ClientKey, IntegerCiphertext, RadixCiphertext};
use tfhe::shortint::ciphertext::{Degree, NoiseLevel};
use tfhe::shortint::client_key::atomic_pattern::{
    AtomicPatternClientKey, KS32AtomicPatternClientKey,
};
use tfhe::shortint::client_key::GenericClientKey;
use tfhe::shortint::{Ciphertext, ClientKey as ShortintClientKey, ShortintParameterSet};

mod protocol;

use protocol::{
    read_response_and_timing, write_request, BatchMetadata, ServerTiming, FRAME_RESPONSE,
    OP_ADD_SCALAR_U8_HPU_NATIVE_ROUNDTRIP, OP_ADD_U8, OP_BITAND_U8, OP_BITNOT_U8, OP_BITOR_U8,
    OP_BITXOR_U8, OP_DIV_SCALAR_U8, OP_DIV_U8, OP_EQ_U8, OP_GE_U8, OP_GT_U8, OP_INPUT_HPU_NATIVE,
    OP_LE_U8, OP_LT_U8, OP_MUL_SCALAR_U8, OP_MUL_U8, OP_NE_U8, OP_OUTPUT_HPU_NATIVE,
    OP_REM_SCALAR_U8, OP_REM_U8, OP_ROTL_SCALAR_U8, OP_ROTL_U8, OP_ROTR_SCALAR_U8, OP_ROTR_U8,
    OP_RSUB_SCALAR_U8, OP_SHL_SCALAR_U8, OP_SHL_U8, OP_SHR_SCALAR_U8, OP_SHR_U8, OP_SUB_SCALAR_U8,
    OP_SUB_U8, VERSION_TIMING,
};

const LBA_BYTES: usize = 4096;
const RADIX_BLOCKS: usize = 4;
const MESSAGE_WIDTH: usize = 2;
const CARRY_WIDTH: usize = 2;
const PADDING_BIT_WIDTH: usize = 1;
const DELTA_LOG2: usize = 59;
const BIG_LWE_DIMENSION: usize = 2048;
const HPU_PC_COUNT: usize = 2;
const HPU_PC_GROUP_WORDS: usize = 16;
const HPU_PC_DATA_WORDS: usize = BIG_LWE_DIMENSION / HPU_PC_COUNT;
const HPU_PC0_DATA_WORDS: usize = HPU_PC_DATA_WORDS + 1;
const HPU_PC_SLOT_WORDS: usize = 3 * LBA_BYTES / size_of::<u64>();
const HPU_NATIVE_LWE_WORDS: usize = HPU_PC_COUNT * HPU_PC_SLOT_WORDS;
const HPU_NATIVE_WORDS_PER_U8: usize = RADIX_BLOCKS * HPU_NATIVE_LWE_WORDS;
const DEFAULT_CLIENT_KEY: &str = "hpu/keys/psi64/psi64_shortint_ks32_client_key.bincode";

type KS32ClientKey = GenericClientKey<KS32AtomicPatternClientKey>;

#[derive(Clone, Copy, Debug, ValueEnum)]
enum StorageBackend {
    /// Fidus SSD exposed to the x86/QEMU environment through NVMQ.
    FidusSsd,
    /// SSD installed in the x86 host, accessed as a block device or regular file.
    X86LocalSsd,
}

impl StorageBackend {
    fn label(self) -> &'static str {
        match self {
            Self::FidusSsd => "fidus-ssd",
            Self::X86LocalSsd => "x86-local-ssd",
        }
    }
}

#[derive(Clone, Copy, Debug, ValueEnum)]
enum RemoteOperation {
    AddScalar,
    SubScalar,
    RsubScalar,
    MulScalar,
    DivScalar,
    RemScalar,
    ShlScalar,
    ShrScalar,
    RotlScalar,
    RotrScalar,
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    Shl,
    Shr,
    Rotl,
    Rotr,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
}

impl RemoteOperation {
    fn wire(self) -> u64 {
        let base = match self {
            Self::AddScalar => return OP_ADD_SCALAR_U8_HPU_NATIVE_ROUNDTRIP,
            Self::SubScalar => OP_SUB_SCALAR_U8,
            Self::RsubScalar => OP_RSUB_SCALAR_U8,
            Self::MulScalar => OP_MUL_SCALAR_U8,
            Self::DivScalar => OP_DIV_SCALAR_U8,
            Self::RemScalar => OP_REM_SCALAR_U8,
            Self::ShlScalar => OP_SHL_SCALAR_U8,
            Self::ShrScalar => OP_SHR_SCALAR_U8,
            Self::RotlScalar => OP_ROTL_SCALAR_U8,
            Self::RotrScalar => OP_ROTR_SCALAR_U8,
            Self::Add => OP_ADD_U8,
            Self::Sub => OP_SUB_U8,
            Self::Mul => OP_MUL_U8,
            Self::Div => OP_DIV_U8,
            Self::Rem => OP_REM_U8,
            Self::BitAnd => OP_BITAND_U8,
            Self::BitOr => OP_BITOR_U8,
            Self::BitXor => OP_BITXOR_U8,
            Self::BitNot => OP_BITNOT_U8,
            Self::Shl => OP_SHL_U8,
            Self::Shr => OP_SHR_U8,
            Self::Rotl => OP_ROTL_U8,
            Self::Rotr => OP_ROTR_U8,
            Self::Eq => OP_EQ_U8,
            Self::Ne => OP_NE_U8,
            Self::Lt => OP_LT_U8,
            Self::Le => OP_LE_U8,
            Self::Gt => OP_GT_U8,
            Self::Ge => OP_GE_U8,
        };
        base | OP_INPUT_HPU_NATIVE | OP_OUTPUT_HPU_NATIVE
    }

    fn label(self) -> &'static str {
        match self {
            Self::AddScalar => "add-scalar",
            Self::SubScalar => "sub-scalar",
            Self::RsubScalar => "rsub-scalar",
            Self::MulScalar => "mul-scalar",
            Self::DivScalar => "div-scalar",
            Self::RemScalar => "rem-scalar",
            Self::ShlScalar => "shl-scalar",
            Self::ShrScalar => "shr-scalar",
            Self::RotlScalar => "rotl-scalar",
            Self::RotrScalar => "rotr-scalar",
            Self::Add => "add",
            Self::Sub => "sub",
            Self::Mul => "mul",
            Self::Div => "div",
            Self::Rem => "rem",
            Self::BitAnd => "bitand",
            Self::BitOr => "bitor",
            Self::BitXor => "bitxor",
            Self::BitNot => "bitnot",
            Self::Shl => "shl",
            Self::Shr => "shr",
            Self::Rotl => "rotl",
            Self::Rotr => "rotr",
            Self::Eq => "eq",
            Self::Ne => "ne",
            Self::Lt => "lt",
            Self::Le => "le",
            Self::Gt => "gt",
            Self::Ge => "ge",
        }
    }

    fn is_binary(self) -> bool {
        matches!(
            self,
            Self::Add
                | Self::Sub
                | Self::Mul
                | Self::Div
                | Self::Rem
                | Self::BitAnd
                | Self::BitOr
                | Self::BitXor
                | Self::Shl
                | Self::Shr
                | Self::Rotl
                | Self::Rotr
                | Self::Eq
                | Self::Ne
                | Self::Lt
                | Self::Le
                | Self::Gt
                | Self::Ge
        )
    }

    fn returns_bool(self) -> bool {
        matches!(
            self,
            Self::Eq | Self::Ne | Self::Lt | Self::Le | Self::Gt | Self::Ge
        )
    }

    fn evaluate(self, lhs: u8, rhs: u8, scalar: u8) -> u8 {
        match self {
            Self::AddScalar => lhs.wrapping_add(scalar),
            Self::SubScalar => lhs.wrapping_sub(scalar),
            Self::RsubScalar => scalar.wrapping_sub(lhs),
            Self::MulScalar => lhs.wrapping_mul(scalar),
            Self::DivScalar => lhs / scalar,
            Self::RemScalar => lhs % scalar,
            Self::ShlScalar => lhs.wrapping_shl(u32::from(scalar)),
            Self::ShrScalar => lhs.wrapping_shr(u32::from(scalar)),
            Self::RotlScalar => lhs.rotate_left(u32::from(scalar)),
            Self::RotrScalar => lhs.rotate_right(u32::from(scalar)),
            Self::Add => lhs.wrapping_add(rhs),
            Self::Sub => lhs.wrapping_sub(rhs),
            Self::Mul => lhs.wrapping_mul(rhs),
            Self::Div => lhs / rhs,
            Self::Rem => lhs % rhs,
            Self::BitAnd => lhs & rhs,
            Self::BitOr => lhs | rhs,
            Self::BitXor => lhs ^ rhs,
            Self::BitNot => !lhs,
            Self::Shl => lhs.wrapping_shl(u32::from(rhs)),
            Self::Shr => lhs.wrapping_shr(u32::from(rhs)),
            Self::Rotl => lhs.rotate_left(u32::from(rhs)),
            Self::Rotr => lhs.rotate_right(u32::from(rhs)),
            Self::Eq => u8::from(lhs == rhs),
            Self::Ne => u8::from(lhs != rhs),
            Self::Lt => u8::from(lhs < rhs),
            Self::Le => u8::from(lhs <= rhs),
            Self::Gt => u8::from(lhs > rhs),
            Self::Ge => u8::from(lhs >= rhs),
        }
    }
}

#[derive(Parser, Debug)]
#[command(
    long_about = "Read u8 plaintext from an x86-visible SSD, encrypt and decrypt with the tfhe-rs CPU path, execute the selected operation on the remote real HPU, and write the clear result back to SSD. The TCP payload uses the same padded HPU-native psi64/V80 layout as the FPGA pipeline."
)]
struct Args {
    /// Storage source label written to logs and CSV output.
    #[arg(long, value_enum, default_value_t = StorageBackend::FidusSsd)]
    storage_backend: StorageBackend,

    /// Source block device or preallocated regular file visible on x86.
    #[arg(long, default_value = "/dev/nvmq0n1")]
    input_ssd: PathBuf,

    /// Source LBA in 4096-byte SUDA units.
    #[arg(long, default_value_t = 65_536)]
    input_lba: u64,

    /// Number of consecutive u8 plaintext values to process.
    #[arg(long, default_value_t = 128)]
    plaintext_bytes: usize,

    /// Destination block device or regular file for the decrypted result.
    #[arg(long, default_value = "/dev/nvmq0n1")]
    output_ssd: PathBuf,

    /// Destination LBA. Keep it separate from input_lba.
    #[arg(long, default_value_t = 131_072)]
    output_lba: u64,

    /// Create output_ssd when it is a regular file that does not exist.
    #[arg(long)]
    create_output_file: bool,

    /// Saved KS32 ClientKey matching the remote psi64 server key.
    #[arg(long, default_value = DEFAULT_CLIENT_KEY)]
    client_key: PathBuf,

    /// Remote HPU TCP service.
    #[arg(long, default_value = "10.16.0.129:19090")]
    server: SocketAddr,

    /// Remote HPU operation.
    #[arg(long, value_enum, default_value_t = RemoteOperation::AddScalar)]
    remote_operation: RemoteOperation,

    /// Clear u8 scalar evaluated by scalar operations.
    #[arg(long, default_value_t = 1)]
    scalar: u8,

    /// For binary operations, encrypt rhs[i] = lhs[i] + rhs_offset.
    #[arg(long, default_value_t = 1)]
    rhs_offset: u8,

    /// CPU workers used for independent u8 encryption, packing, unpacking and decryption.
    #[arg(long, default_value_t = 1)]
    cpu_threads: usize,

    #[arg(long, default_value_t = 10_000)]
    connect_timeout_ms: u64,

    #[arg(long, default_value_t = 3_600)]
    io_timeout_secs: u64,

    #[arg(long, default_value_t = 4 * 1024 * 1024 * 1024)]
    max_response_bytes: usize,

    /// Optional expected first plaintext byte before scalar addition.
    #[arg(long)]
    expect: Option<u8>,

    /// Do not read the destination SSD back after writing it.
    #[arg(long)]
    skip_ssd_readback: bool,

    /// Append this run to a CSV file.
    #[arg(long)]
    csv: Option<PathBuf>,
}

#[derive(Default)]
struct LocalTiming {
    key_load: Duration,
    ssd_read: Duration,
    cpu_encrypt: Duration,
    cpu_native_pack: Duration,
    tcp_connect: Duration,
    request_send: Duration,
    response_header: Duration,
    response_receive: Duration,
    telemetry_receive: Duration,
    cpu_native_unpack: Duration,
    cpu_decrypt: Duration,
    ssd_write: Duration,
    ssd_readback: Duration,
    online_e2e: Duration,
    process: Duration,
}

fn main() -> ExitCode {
    match run(Args::parse()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("CPU baseline failed: {err}");
            ExitCode::from(1)
        }
    }
}

fn run(args: Args) -> Result<(), String> {
    let process_start = Instant::now();
    validate_args(&args)?;
    if args.cpu_threads > 1 {
        rayon::ThreadPoolBuilder::new()
            .num_threads(args.cpu_threads)
            .build_global()
            .map_err(|err| format!("unable to create CPU thread pool: {err}"))?;
    }
    let mut timing = LocalTiming::default();

    let key_start = Instant::now();
    let serialized_key = fs::read(&args.client_key)
        .map_err(|err| format!("unable to read {}: {err}", args.client_key.display()))?;
    let ks32_client_key: KS32ClientKey = bincode::deserialize(&serialized_key)
        .map_err(|err| format!("unable to deserialize {}: {err}", args.client_key.display()))?;
    let params: ShortintParameterSet = ks32_client_key.parameters();
    validate_parameters(&params)?;
    let shortint_client_key = ShortintClientKey {
        atomic_pattern: AtomicPatternClientKey::KeySwitch32(ks32_client_key.atomic_pattern),
    };
    let client_key = ClientKey::from_raw_parts(shortint_client_key);
    timing.key_load = key_start.elapsed();

    let online_start = Instant::now();
    let io_bytes = round_up_lba(args.plaintext_bytes)?;
    let mut input_block = vec![0_u8; io_bytes];
    let read_start = Instant::now();
    let input_file = File::open(&args.input_ssd)
        .map_err(|err| format!("unable to open {}: {err}", args.input_ssd.display()))?;
    read_exact_at(&input_file, &mut input_block, lba_offset(args.input_lba)?)?;
    timing.ssd_read = read_start.elapsed();
    let plaintext = input_block[..args.plaintext_bytes].to_vec();
    if let Some(expected) = args.expect {
        if plaintext[0] != expected {
            return Err(format!(
                "source first byte mismatch: SSD={}, expected={expected}",
                plaintext[0]
            ));
        }
    }

    let rhs_plaintext = args.remote_operation.is_binary().then(|| {
        plaintext
            .iter()
            .map(|value| value.wrapping_add(args.rhs_offset))
            .collect::<Vec<_>>()
    });
    let encrypt_start = Instant::now();
    let ciphertexts = encrypt_batch(&client_key, &plaintext, args.cpu_threads);
    let rhs_ciphertexts = rhs_plaintext
        .as_ref()
        .map(|values| encrypt_batch(&client_key, values, args.cpu_threads));
    timing.cpu_encrypt = encrypt_start.elapsed();

    let metadata = native_metadata(args.plaintext_bytes)?;
    let operand_count = if args.remote_operation.is_binary() {
        2
    } else {
        1
    };
    let request_word_count = metadata
        .ciphertext_word_count
        .checked_mul(operand_count)
        .ok_or_else(|| "multi-operand request size overflow".to_string())?;
    let pack_start = Instant::now();
    let mut native_words = vec![0_u64; request_word_count];
    let (lhs_words, rhs_words) = native_words.split_at_mut(metadata.ciphertext_word_count);
    pack_batch_hpu_native(&ciphertexts, args.cpu_threads, lhs_words)?;
    if let (Some(rhs_ciphertexts), Some(rhs_words)) = (
        rhs_ciphertexts.as_ref(),
        (!rhs_words.is_empty()).then_some(rhs_words),
    ) {
        pack_batch_hpu_native(rhs_ciphertexts, args.cpu_threads, rhs_words)?;
    }
    timing.cpu_native_pack = pack_start.elapsed();

    let request_id = request_id();
    let connect_start = Instant::now();
    let mut stream =
        TcpStream::connect_timeout(&args.server, Duration::from_millis(args.connect_timeout_ms))
            .map_err(|err| format!("unable to connect to {}: {err}", args.server))?;
    stream
        .set_read_timeout(Some(Duration::from_secs(args.io_timeout_secs)))
        .map_err(|err| format!("unable to set TCP read timeout: {err}"))?;
    stream
        .set_write_timeout(Some(Duration::from_secs(args.io_timeout_secs)))
        .map_err(|err| format!("unable to set TCP write timeout: {err}"))?;
    stream
        .set_nodelay(true)
        .map_err(|err| format!("unable to enable TCP_NODELAY: {err}"))?;
    timing.tcp_connect = connect_start.elapsed();

    let rpc_start = Instant::now();
    let send_start = Instant::now();
    write_request(
        &mut stream,
        request_id,
        args.remote_operation.wire(),
        args.scalar,
        &metadata,
        &native_words,
    )?;
    timing.request_send = send_start.elapsed();
    drop(native_words);

    let (response, server_timing, receive_timing) =
        read_response_and_timing(&mut stream, args.max_response_bytes)?;
    let rpc_round_trip = rpc_start.elapsed();
    timing.response_header = receive_timing.response_header;
    timing.response_receive = receive_timing.response_payload;
    timing.telemetry_receive = receive_timing.telemetry;
    validate_response(
        &response,
        &metadata,
        request_id,
        args.remote_operation,
        args.scalar,
        &server_timing,
    )?;

    let unpack_start = Instant::now();
    let result_ciphertexts = unpack_batch_hpu_native(
        &response.ciphertext_words,
        args.plaintext_bytes,
        response.metadata.radix_blocks_per_item,
        args.cpu_threads,
        &params,
    )?;
    timing.cpu_native_unpack = unpack_start.elapsed();

    let decrypt_start = Instant::now();
    let clear_results = decrypt_batch(&client_key, &result_ciphertexts, args.cpu_threads);
    timing.cpu_decrypt = decrypt_start.elapsed();
    let expected_results: Vec<u8> = plaintext
        .iter()
        .enumerate()
        .map(|(index, lhs)| {
            let rhs = rhs_plaintext.as_ref().map_or(0, |values| values[index]);
            args.remote_operation.evaluate(*lhs, rhs, args.scalar)
        })
        .collect();
    if clear_results != expected_results {
        let mismatch = clear_results
            .iter()
            .zip(&expected_results)
            .position(|(actual, expected)| actual != expected)
            .unwrap_or(0);
        return Err(format!(
            "CPU decrypt mismatch at item {mismatch}: actual={}, expected={}",
            clear_results[mismatch], expected_results[mismatch]
        ));
    }

    let mut output_block = vec![0_u8; io_bytes];
    output_block[..clear_results.len()].copy_from_slice(&clear_results);
    let write_start = Instant::now();
    let output_file = OpenOptions::new()
        .read(true)
        .write(true)
        .create(args.create_output_file)
        .open(&args.output_ssd)
        .map_err(|err| format!("unable to open {}: {err}", args.output_ssd.display()))?;
    write_all_at(&output_file, &output_block, lba_offset(args.output_lba)?)?;
    timing.ssd_write = write_start.elapsed();

    if !args.skip_ssd_readback {
        let readback_start = Instant::now();
        let mut readback = vec![0_u8; io_bytes];
        read_exact_at(&output_file, &mut readback, lba_offset(args.output_lba)?)?;
        timing.ssd_readback = readback_start.elapsed();
        if readback != output_block {
            let mismatch = readback
                .iter()
                .zip(&output_block)
                .position(|(actual, expected)| actual != expected)
                .unwrap_or(0);
            return Err(format!(
                "destination SSD readback mismatch at byte {mismatch}: actual=0x{:02x}, expected=0x{:02x}",
                readback[mismatch], output_block[mismatch]
            ));
        }
    }
    timing.online_e2e = online_start.elapsed();
    timing.process = process_start.elapsed();

    print_result(
        &args,
        &plaintext,
        &clear_results,
        request_id,
        rpc_round_trip,
        &timing,
        &server_timing,
        response.ciphertext_words.len() * size_of::<u64>(),
    );
    if let Some(path) = &args.csv {
        append_csv(
            path,
            &args,
            request_id,
            rpc_round_trip,
            &timing,
            &server_timing,
        )?;
    }
    Ok(())
}

fn validate_args(args: &Args) -> Result<(), String> {
    if args.plaintext_bytes == 0 {
        return Err("--plaintext-bytes must be greater than zero".to_string());
    }
    if args.cpu_threads == 0 {
        return Err("--cpu-threads must be greater than zero".to_string());
    }
    if args.input_ssd == args.output_ssd && args.input_lba == args.output_lba {
        return Err("input and output SSD LBAs must not overlap".to_string());
    }
    Ok(())
}

fn validate_parameters(params: &ShortintParameterSet) -> Result<(), String> {
    let big_lwe_dimension = params.glwe_dimension().0 * params.polynomial_size().0;
    if big_lwe_dimension != BIG_LWE_DIMENSION
        || params.message_modulus().0 != 1_u64 << MESSAGE_WIDTH
        || params.carry_modulus().0 != 1_u64 << CARRY_WIDTH
        || params.encryption_lwe_dimension().0 != BIG_LWE_DIMENSION
    {
        return Err(format!(
            "ClientKey is not the psi64 shape: big_lwe_dimension={big_lwe_dimension}, encryption_lwe_dimension={}, message_modulus={}, carry_modulus={}",
            params.encryption_lwe_dimension().0,
            params.message_modulus().0,
            params.carry_modulus().0
        ));
    }
    Ok(())
}

fn native_metadata(item_count: usize) -> Result<BatchMetadata, String> {
    let ciphertext_word_count = item_count
        .checked_mul(HPU_NATIVE_WORDS_PER_U8)
        .ok_or_else(|| "ciphertext batch size overflow".to_string())?;
    Ok(BatchMetadata {
        mask_dimension: BIG_LWE_DIMENSION,
        item_count,
        radix_blocks_per_item: RADIX_BLOCKS,
        message_width: MESSAGE_WIDTH,
        carry_width: CARRY_WIDTH,
        padding_bit_width: PADDING_BIT_WIDTH,
        delta_log2: DELTA_LOG2,
        ciphertext_word_count,
    })
}

fn encrypt_batch(client_key: &ClientKey, values: &[u8], threads: usize) -> Vec<RadixCiphertext> {
    if threads == 1 {
        values
            .iter()
            .map(|value| client_key.encrypt_radix(*value, RADIX_BLOCKS))
            .collect()
    } else {
        values
            .par_iter()
            .map(|value| client_key.encrypt_radix(*value, RADIX_BLOCKS))
            .collect()
    }
}

fn decrypt_batch(
    client_key: &ClientKey,
    ciphertexts: &[RadixCiphertext],
    threads: usize,
) -> Vec<u8> {
    if threads == 1 {
        ciphertexts
            .iter()
            .map(|ciphertext| client_key.decrypt_radix(ciphertext))
            .collect()
    } else {
        ciphertexts
            .par_iter()
            .map(|ciphertext| client_key.decrypt_radix(ciphertext))
            .collect()
    }
}

fn reverse_psi64_mask_index(index: usize) -> usize {
    let mut value = index;
    let mut reversed = 0;
    for _ in 0..11 {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
    }
    reversed
}

fn pack_batch_hpu_native(
    ciphertexts: &[RadixCiphertext],
    threads: usize,
    output: &mut [u64],
) -> Result<(), String> {
    if output.len() != ciphertexts.len() * HPU_NATIVE_WORDS_PER_U8 {
        return Err("HPU-native packing output size mismatch".to_string());
    }
    if threads == 1 {
        for (ciphertext, native) in ciphertexts
            .iter()
            .zip(output.chunks_exact_mut(HPU_NATIVE_WORDS_PER_U8))
        {
            pack_radix_hpu_native(ciphertext, native)?;
        }
    } else {
        ciphertexts
            .par_iter()
            .zip(output.par_chunks_exact_mut(HPU_NATIVE_WORDS_PER_U8))
            .try_for_each(|(ciphertext, native)| pack_radix_hpu_native(ciphertext, native))?;
    }
    Ok(())
}

fn pack_radix_hpu_native(ciphertext: &RadixCiphertext, native: &mut [u64]) -> Result<(), String> {
    if ciphertext.blocks().len() != RADIX_BLOCKS {
        return Err(format!(
            "radix block count mismatch: got={}, expected={RADIX_BLOCKS}",
            ciphertext.blocks().len()
        ));
    }
    native.fill(0);
    for (block, native_lwe) in ciphertext
        .blocks()
        .iter()
        .zip(native.chunks_exact_mut(HPU_NATIVE_LWE_WORDS))
    {
        let logical = block.ct.as_ref();
        if logical.len() != BIG_LWE_DIMENSION + 1 {
            return Err(format!(
                "Big-LWE word count mismatch: got={}, expected={}",
                logical.len(),
                BIG_LWE_DIMENSION + 1
            ));
        }
        for (natural_index, word) in logical[..BIG_LWE_DIMENSION].iter().enumerate() {
            let hpu_index = reverse_psi64_mask_index(natural_index);
            let group = hpu_index / HPU_PC_GROUP_WORDS;
            let lane = hpu_index % HPU_PC_GROUP_WORDS;
            let pc = group % HPU_PC_COUNT;
            let pc_offset = (group / HPU_PC_COUNT) * HPU_PC_GROUP_WORDS + lane;
            native_lwe[pc * HPU_PC_SLOT_WORDS + pc_offset] = *word;
        }
        native_lwe[HPU_PC_DATA_WORDS] = logical[BIG_LWE_DIMENSION];
    }
    Ok(())
}

fn unpack_batch_hpu_native(
    words: &[u64],
    item_count: usize,
    blocks_per_item: usize,
    threads: usize,
    params: &ShortintParameterSet,
) -> Result<Vec<RadixCiphertext>, String> {
    let words_per_item = blocks_per_item
        .checked_mul(HPU_NATIVE_LWE_WORDS)
        .ok_or_else(|| "HPU-native result shape overflow".to_string())?;
    let expected_words = item_count
        .checked_mul(words_per_item)
        .ok_or_else(|| "HPU-native result batch overflow".to_string())?;
    if words.len() != expected_words {
        return Err(format!(
            "HPU-native response size mismatch: words={}, expected={expected_words}",
            words.len()
        ));
    }
    let unpack = |native: &[u64]| unpack_radix_hpu_native(native, blocks_per_item, params);
    if threads == 1 {
        words.chunks_exact(words_per_item).map(unpack).collect()
    } else {
        words.par_chunks_exact(words_per_item).map(unpack).collect()
    }
}

fn unpack_radix_hpu_native(
    native: &[u64],
    blocks_per_item: usize,
    params: &ShortintParameterSet,
) -> Result<RadixCiphertext, String> {
    let mut blocks = Vec::with_capacity(blocks_per_item);
    for (block_index, native_lwe) in native.chunks_exact(HPU_NATIVE_LWE_WORDS).enumerate() {
        if native_lwe[HPU_PC0_DATA_WORDS..HPU_PC_SLOT_WORDS]
            .iter()
            .chain(native_lwe[HPU_PC_SLOT_WORDS + HPU_PC_DATA_WORDS..].iter())
            .any(|word| *word != 0)
        {
            return Err(format!(
                "non-zero HPU-native padding in radix block {block_index}"
            ));
        }
        let mut logical = vec![0_u64; BIG_LWE_DIMENSION + 1];
        for (natural_index, word) in logical[..BIG_LWE_DIMENSION].iter_mut().enumerate() {
            let hpu_index = reverse_psi64_mask_index(natural_index);
            let group = hpu_index / HPU_PC_GROUP_WORDS;
            let lane = hpu_index % HPU_PC_GROUP_WORDS;
            let pc = group % HPU_PC_COUNT;
            let pc_offset = (group / HPU_PC_COUNT) * HPU_PC_GROUP_WORDS + lane;
            *word = native_lwe[pc * HPU_PC_SLOT_WORDS + pc_offset];
        }
        logical[BIG_LWE_DIMENSION] = native_lwe[HPU_PC_DATA_WORDS];
        let lwe = LweCiphertextOwned::from_container(logical, params.ciphertext_modulus());
        blocks.push(Ciphertext::new(
            lwe,
            Degree::new(params.message_modulus().0 - 1),
            NoiseLevel::NOMINAL,
            params.message_modulus(),
            params.carry_modulus(),
            params.atomic_pattern(),
        ));
    }
    Ok(RadixCiphertext::from(blocks))
}

fn validate_response(
    response: &protocol::CiphertextFrame,
    request_metadata: &BatchMetadata,
    request_id: u64,
    operation: RemoteOperation,
    scalar: u8,
    timing: &ServerTiming,
) -> Result<(), String> {
    let mut expected_metadata = request_metadata.clone();
    if operation.returns_bool() {
        expected_metadata.radix_blocks_per_item = 1;
        expected_metadata.ciphertext_word_count = expected_metadata
            .item_count
            .checked_mul(HPU_NATIVE_LWE_WORDS)
            .ok_or_else(|| "comparison response size overflow".to_string())?;
    }
    if response.version != VERSION_TIMING
        || response.kind != FRAME_RESPONSE
        || response.request_id != request_id
        || response.operation != operation.wire()
        || response.status != 0
        || response.scalar != u64::from(scalar)
        || response.metadata != expected_metadata
    {
        return Err(format!("remote response metadata mismatch: {response:?}"));
    }
    if response.error_message.is_some() {
        return Err("unexpected error payload in successful response".to_string());
    }
    if timing.request_id != request_id {
        return Err(format!(
            "server timing request_id mismatch: timing={}, request={request_id}",
            timing.request_id
        ));
    }
    let response_payload_bytes = u64::try_from(response.ciphertext_words.len())
        .ok()
        .and_then(|words| words.checked_mul(size_of::<u64>() as u64))
        .ok_or_else(|| "response payload byte count overflow".to_string())?;
    if timing.response_payload_bytes != response_payload_bytes {
        return Err(format!(
            "server timing payload mismatch: timing={}, response={response_payload_bytes}",
            timing.response_payload_bytes
        ));
    }
    Ok(())
}

fn round_up_lba(bytes: usize) -> Result<usize, String> {
    bytes
        .checked_add(LBA_BYTES - 1)
        .map(|value| value / LBA_BYTES * LBA_BYTES)
        .ok_or_else(|| "SSD I/O size overflow".to_string())
}

fn lba_offset(lba: u64) -> Result<u64, String> {
    lba.checked_mul(LBA_BYTES as u64)
        .ok_or_else(|| "SSD LBA offset overflow".to_string())
}

fn read_exact_at(file: &File, output: &mut [u8], offset: u64) -> Result<(), String> {
    let mut done = 0;
    while done < output.len() {
        let read = file
            .read_at(&mut output[done..], offset + done as u64)
            .map_err(|err| format!("SSD read failed at byte {}: {err}", offset + done as u64))?;
        if read == 0 {
            return Err(format!(
                "unexpected end of SSD at byte {}",
                offset + done as u64
            ));
        }
        done += read;
    }
    Ok(())
}

fn write_all_at(file: &File, input: &[u8], offset: u64) -> Result<(), String> {
    let mut done = 0;
    while done < input.len() {
        let written = file
            .write_at(&input[done..], offset + done as u64)
            .map_err(|err| format!("SSD write failed at byte {}: {err}", offset + done as u64))?;
        if written == 0 {
            return Err(format!(
                "zero-length SSD write at byte {}",
                offset + done as u64
            ));
        }
        done += written;
    }
    Ok(())
}

fn request_id() -> u64 {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    (nanos as u64) ^ ((nanos >> 64) as u64)
}

fn ms(duration: Duration) -> f64 {
    duration.as_secs_f64() * 1000.0
}

fn ns_ms(value: u64) -> f64 {
    value as f64 / 1_000_000.0
}

#[allow(clippy::too_many_arguments)]
fn print_result(
    args: &Args,
    plaintext: &[u8],
    clear_results: &[u8],
    request_id: u64,
    rpc_round_trip: Duration,
    timing: &LocalTiming,
    server: &ServerTiming,
    result_bytes: usize,
) {
    println!("lwe CPU baseline SSD-to-remote-HPU-to-SSD pipeline passed");
    println!(
        "data_path=SSD->Host_memory->CPU_encrypt->CPU_HPU_native_pack->TCP->remote_HPU->TCP->CPU_HPU_native_unpack->CPU_decrypt->Host_memory->SSD"
    );
    println!(
        "storage_backend={} input_ssd={} input_lba={} output_ssd={} output_lba={} plaintext_bytes={}",
        args.storage_backend.label(),
        args.input_ssd.display(),
        args.input_lba,
        args.output_ssd.display(),
        args.output_lba,
        args.plaintext_bytes
    );
    println!(
        "plaintext_prefix={} result_prefix={}",
        hex_prefix(plaintext),
        hex_prefix(clear_results)
    );
    println!(
        "cpu_threads={} client_key={} wire_layout=hpu-native-psi64-v80 request_bytes={} result_bytes={}",
        args.cpu_threads,
        args.client_key.display(),
        args.plaintext_bytes
            * HPU_NATIVE_WORDS_PER_U8
            * 8
            * if args.remote_operation.is_binary() { 2 } else { 1 },
        result_bytes
    );
    println!(
        "remote_server={} request_id={} operation={} scalar={} rhs_offset={}",
        args.server,
        request_id,
        args.remote_operation.label(),
        args.scalar,
        args.rhs_offset
    );
    println!(
        "benchmark_stage_ms key_load={:.3} ssd_read={:.3} cpu_encrypt={:.3} cpu_native_pack={:.3} tcp_connect={:.3} request_send={:.3} wait_response_header={:.3} response_receive={:.3} telemetry_receive={:.3} rpc_round_trip={:.3} cpu_native_unpack={:.3} cpu_decrypt={:.3} ssd_write={:.3} ssd_readback={:.3} online_e2e={:.3} process={:.3}",
        ms(timing.key_load),
        ms(timing.ssd_read),
        ms(timing.cpu_encrypt),
        ms(timing.cpu_native_pack),
        ms(timing.tcp_connect),
        ms(timing.request_send),
        ms(timing.response_header),
        ms(timing.response_receive),
        ms(timing.telemetry_receive),
        ms(rpc_round_trip),
        ms(timing.cpu_native_unpack),
        ms(timing.cpu_decrypt),
        ms(timing.ssd_write),
        ms(timing.ssd_readback),
        ms(timing.online_e2e),
        ms(timing.process)
    );
    println!(
        "remote_stage_ms request_receive={:.3} validate={:.3} decode={:.3} hpu_prepare={:.3} hpu_enqueue={:.3} hpu_wait_sync={:.3} hpu_output_convert={:.3} result_encode={:.3} mem_sanitizer={:.3} process={:.3} response_send={:.3} server_total={:.3}",
        ns_ms(server.request_receive_ns),
        ns_ms(server.request_validate_ns),
        ns_ms(server.request_decode_ns),
        ns_ms(server.hpu_prepare_ns),
        ns_ms(server.hpu_enqueue_ns),
        ns_ms(server.hpu_wait_sync_ns),
        ns_ms(server.hpu_output_convert_ns),
        ns_ms(server.result_encode_ns),
        ns_ms(server.mem_sanitizer_ns),
        ns_ms(server.remote_process_ns),
        ns_ms(server.response_send_ns),
        ns_ms(server.server_total_ns)
    );
    println!(
        "BENCH_CPU_FULL_PIPELINE_CSV_V2,{},{},{},{},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6},{:.6}",
        args.storage_backend.label(),
        args.plaintext_bytes,
        args.cpu_threads,
        request_id,
        ms(timing.ssd_read),
        ms(timing.cpu_encrypt),
        ms(timing.cpu_native_pack),
        ms(timing.tcp_connect),
        ms(timing.request_send),
        ms(timing.response_header),
        ms(timing.response_receive),
        ms(rpc_round_trip),
        ns_ms(server.hpu_prepare_ns),
        ns_ms(server.hpu_wait_sync_ns),
        ns_ms(server.hpu_output_convert_ns),
        ms(timing.cpu_native_unpack),
        ms(timing.cpu_decrypt),
        ms(timing.ssd_write),
        ms(timing.ssd_readback),
        ms(timing.online_e2e),
        ms(timing.process),
        ns_ms(server.server_total_ns)
    );
    println!("cpu_encrypt_decrypt_checked=yes");
    println!("remote_hpu_ciphertext_compute=passed");
    println!(
        "destination_ssd_readback_checked={}",
        if args.skip_ssd_readback { "no" } else { "yes" }
    );
}

fn hex_prefix(bytes: &[u8]) -> String {
    let mut text = bytes
        .iter()
        .take(16)
        .map(|value| format!("{value:02x}"))
        .collect::<String>();
    if bytes.len() > 16 {
        text.push_str("...");
    }
    text
}

fn append_csv(
    path: &Path,
    args: &Args,
    request_id: u64,
    rpc: Duration,
    timing: &LocalTiming,
    server: &ServerTiming,
) -> Result<(), String> {
    let needs_header = !path.exists()
        || fs::metadata(path)
            .map_err(|err| format!("unable to stat {}: {err}", path.display()))?
            .len()
            == 0;
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(path)
        .map_err(|err| format!("unable to open CSV {}: {err}", path.display()))?;
    if needs_header {
        writeln!(
            file,
            "storage_backend,input_path,input_lba,output_path,output_lba,plaintext_bytes,cpu_threads,request_id,ssd_read_ms,cpu_encrypt_ms,cpu_native_pack_ms,tcp_connect_ms,request_send_ms,wait_response_header_ms,response_receive_ms,rpc_round_trip_ms,remote_hpu_prepare_ms,remote_hpu_wait_sync_ms,remote_hpu_output_convert_ms,cpu_native_unpack_ms,cpu_decrypt_ms,ssd_write_ms,ssd_readback_ms,online_e2e_ms,process_ms,remote_server_total_ms"
        )
        .map_err(|err| format!("unable to write CSV header: {err}"))?;
    }
    writeln!(
        file,
        "{},{},{},{},{},{},{},{},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9}",
        args.storage_backend.label(),
        args.input_ssd.display(),
        args.input_lba,
        args.output_ssd.display(),
        args.output_lba,
        args.plaintext_bytes,
        args.cpu_threads,
        request_id,
        ms(timing.ssd_read),
        ms(timing.cpu_encrypt),
        ms(timing.cpu_native_pack),
        ms(timing.tcp_connect),
        ms(timing.request_send),
        ms(timing.response_header),
        ms(timing.response_receive),
        ms(rpc),
        ns_ms(server.hpu_prepare_ns),
        ns_ms(server.hpu_wait_sync_ns),
        ns_ms(server.hpu_output_convert_ns),
        ms(timing.cpu_native_unpack),
        ms(timing.cpu_decrypt),
        ms(timing.ssd_write),
        ms(timing.ssd_readback),
        ms(timing.online_e2e),
        ms(timing.process),
        ns_ms(server.server_total_ns)
    )
    .map_err(|err| format!("unable to append CSV {}: {err}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bit_reversal_is_an_involution() {
        for index in 0..BIG_LWE_DIMENSION {
            assert_eq!(
                reverse_psi64_mask_index(reverse_psi64_mask_index(index)),
                index
            );
        }
    }

    #[test]
    fn native_size_matches_fpga_pipeline() {
        assert_eq!(HPU_NATIVE_WORDS_PER_U8 * 8, 98_304);
        assert_eq!(
            native_metadata(128).unwrap().ciphertext_word_count * 8,
            12_582_912
        );
    }
}
