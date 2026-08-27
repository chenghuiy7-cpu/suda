use clap::Parser;
use serde::Serialize;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::ExitCode;
use tfhe::integer::{ClientKey, CompressedServerKey};
use tfhe::shortint::client_key::atomic_pattern::AtomicPatternClientKey;
use tfhe::shortint::client_key::atomic_pattern::KS32AtomicPatternClientKey;
use tfhe::shortint::client_key::GenericClientKey;
use tfhe::shortint::parameters::KeySwitch32PBSParameters;
use tfhe::shortint::ClientKey as ShortintClientKey;
use tfhe_hpu_backend::prelude::*;

const DEFAULT_PARAMS: &str =
    "hpu/tfhe-rs/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml";
const DEFAULT_OUTPUT_DIR: &str = "hpu/keys/psi64";

type KS32ClientKey = GenericClientKey<KS32AtomicPatternClientKey>;

#[derive(Parser, Debug, Clone)]
#[command(
    long_about = "Generate a matched psi64 KS32 keyset for the SUDA LWE operators and remote HPU service."
)]
struct Args {
    /// Offline HPU parameter file from the pinned TFHE-rs submodule.
    #[arg(long, default_value = DEFAULT_PARAMS)]
    params: PathBuf,

    /// Output directory for the generated keyset.
    #[arg(long, default_value = DEFAULT_OUTPUT_DIR)]
    output_dir: PathBuf,

    #[arg(long, default_value = "psi64_shortint_ks32_client_key.bincode")]
    client_key_file: PathBuf,

    #[arg(long, default_value = "psi64_big_lwe_secret_key.bin")]
    big_lwe_key_file: PathBuf,

    #[arg(long, default_value = "psi64_integer_compressed_server_key.bincode")]
    server_key_file: PathBuf,

    #[arg(long, default_value = "psi64_key_manifest.txt")]
    manifest_file: PathBuf,

    /// Overwrite existing key files.
    #[arg(long)]
    force: bool,

    /// Call fsync after writing each output file.
    #[arg(long)]
    sync_write: bool,
}

#[derive(Serialize)]
struct Manifest<'a> {
    params_file: &'a Path,
    client_key_file: &'a Path,
    big_lwe_key_file: &'a Path,
    server_key_file: &'a Path,
    lwe_dimension: usize,
    glwe_dimension: usize,
    polynomial_size: usize,
    encryption_lwe_dimension: usize,
    message_modulus: u64,
    carry_modulus: u64,
    ciphertext_modulus_width: usize,
    big_lwe_key_ones: usize,
}

fn main() -> ExitCode {
    match run(Args::parse()) {
        Ok(()) => ExitCode::SUCCESS,
        Err(err) => {
            eprintln!("key generation failed: {err}");
            ExitCode::from(1)
        }
    }
}

fn run(args: Args) -> Result<(), String> {
    fs::create_dir_all(&args.output_dir)
        .map_err(|err| format!("unable to create {:?}: {err}", args.output_dir))?;

    let client_key_path = args.output_dir.join(&args.client_key_file);
    let big_lwe_key_path = args.output_dir.join(&args.big_lwe_key_file);
    let server_key_path = args.output_dir.join(&args.server_key_file);
    let manifest_path = args.output_dir.join(&args.manifest_file);

    for path in [
        &client_key_path,
        &big_lwe_key_path,
        &server_key_path,
        &manifest_path,
    ] {
        check_output_path(path, args.force)?;
    }

    let params_path = args
        .params
        .to_str()
        .ok_or_else(|| format!("parameter path is not valid UTF-8: {:?}", args.params))?;
    let hpu_params = HpuParameters::from_toml(params_path);
    let params = KeySwitch32PBSParameters::from(&hpu_params);
    let client_key = KS32ClientKey {
        atomic_pattern: KS32AtomicPatternClientKey::new(params),
    };
    let big_lwe_key = client_key.atomic_pattern.large_lwe_secret_key();
    let big_lwe_key_bytes: Vec<u8> = big_lwe_key
        .as_ref()
        .iter()
        .map(|&coefficient| {
            assert!(coefficient == 0 || coefficient == 1);
            coefficient as u8
        })
        .collect();
    let shortint_client_key = ShortintClientKey {
        atomic_pattern: AtomicPatternClientKey::KeySwitch32(client_key.atomic_pattern.clone()),
    };
    let integer_client_key = ClientKey::from_raw_parts(shortint_client_key);
    let compressed_server_key =
        CompressedServerKey::new_radix_compressed_server_key(&integer_client_key);

    write_bincode(&client_key_path, &client_key, args.sync_write)?;
    write_all(&big_lwe_key_path, &big_lwe_key_bytes, args.sync_write)?;
    write_bincode(&server_key_path, &compressed_server_key, args.sync_write)?;

    let manifest = Manifest {
        params_file: &args.params,
        client_key_file: &client_key_path,
        big_lwe_key_file: &big_lwe_key_path,
        server_key_file: &server_key_path,
        lwe_dimension: params.lwe_dimension().0,
        glwe_dimension: params.glwe_dimension().0,
        polynomial_size: params.polynomial_size().0,
        encryption_lwe_dimension: params.encryption_lwe_dimension().0,
        message_modulus: params.message_modulus().0,
        carry_modulus: params.carry_modulus().0,
        ciphertext_modulus_width: hpu_params.pbs_params.ciphertext_width,
        big_lwe_key_ones: big_lwe_key_bytes.iter().filter(|&&bit| bit != 0).count(),
    };
    write_all(
        &manifest_path,
        manifest_to_text(&manifest).as_bytes(),
        args.sync_write,
    )?;

    println!("params_file={}", manifest.params_file.display());
    println!("client_key_file={}", client_key_path.display());
    println!("big_lwe_key_file={}", big_lwe_key_path.display());
    println!("server_key_file={}", server_key_path.display());
    println!("manifest_file={}", manifest_path.display());
    println!("lwe_dimension={}", manifest.lwe_dimension);
    println!(
        "encryption_lwe_dimension={}",
        manifest.encryption_lwe_dimension
    );
    println!("big_lwe_key_bytes={}", big_lwe_key_bytes.len());
    println!("big_lwe_key_ones={}", manifest.big_lwe_key_ones);

    Ok(())
}

fn check_output_path(path: &Path, force: bool) -> Result<(), String> {
    if path.exists() && !force {
        return Err(format!(
            "{path:?} already exists, pass --force to overwrite"
        ));
    }
    Ok(())
}

fn write_bincode<T: Serialize>(path: &Path, value: &T, sync: bool) -> Result<(), String> {
    let bytes = bincode::serialize(value)
        .map_err(|err| format!("unable to serialize bincode {path:?}: {err}"))?;
    write_all(path, &bytes, sync)
}

fn write_all(path: &Path, bytes: &[u8], sync: bool) -> Result<(), String> {
    let mut file =
        fs::File::create(path).map_err(|err| format!("unable to create {path:?}: {err}"))?;
    file.write_all(bytes)
        .map_err(|err| format!("unable to write {path:?}: {err}"))?;
    if sync {
        file.sync_all()
            .map_err(|err| format!("unable to fsync {path:?}: {err}"))?;
    }
    Ok(())
}

fn manifest_to_text(manifest: &Manifest<'_>) -> String {
    format!(
        concat!(
            "params_file={}\n",
            "client_key_file={}\n",
            "big_lwe_key_file={}\n",
            "server_key_file={}\n",
            "lwe_dimension={}\n",
            "glwe_dimension={}\n",
            "polynomial_size={}\n",
            "encryption_lwe_dimension={}\n",
            "message_modulus={}\n",
            "carry_modulus={}\n",
            "ciphertext_modulus_width={}\n",
            "big_lwe_key_ones={}\n"
        ),
        manifest.params_file.display(),
        manifest.client_key_file.display(),
        manifest.big_lwe_key_file.display(),
        manifest.server_key_file.display(),
        manifest.lwe_dimension,
        manifest.glwe_dimension,
        manifest.polynomial_size,
        manifest.encryption_lwe_dimension,
        manifest.message_modulus,
        manifest.carry_modulus,
        manifest.ciphertext_modulus_width,
        manifest.big_lwe_key_ones
    )
}
