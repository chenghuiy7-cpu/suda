# LWE test artifacts

This directory intentionally does not track secret keys, serialized TFHE keys,
or ciphertext dumps. Install a matching psi64 artifact set before running the
HLS or host-side correctness tests.

Expected filenames:

- `psi64_big_lwe_secret_key.bin`
- `psi64_shortint_ks32_client_key.bincode`
- `psi64_integer_compressed_server_key.bincode`

The files must be generated from the same TFHE client key. Keep their SHA-256
manifest with the private laboratory artifact bundle rather than committing the
key material to Git.
