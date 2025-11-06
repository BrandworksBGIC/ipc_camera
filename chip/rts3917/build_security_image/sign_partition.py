#!/usr/bin/env python3

"""
HSM Partition File Signing Script (Python 3 Version)
Supports multiple partition types with appropriate signing mechanisms
All signatures are stored in a unified signatures/ directory
"""

import argparse
import os
import sys
import subprocess
import json
import requests
from pathlib import Path
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ed25519
from cryptography.hazmat.backends import default_backend
import base64
import hashlib

SERVER_URL = "http://localhost:8080"
TOKEN = "4d0a780cf562a217d432bbda9fb1837db10d8e5ec5e033f51fac808c36a7e35a"
DEFAULT_KEY_NAME = "partition_key"
DEFAULT_KEY_SIZE = 2048
PUBLIC_KEY_DIR = "public_key"

def show_help():
    """Display help information"""
    print("=== HSM Partition File Signing Tool ===")
    print()
    print("Supports the following signing mechanisms:")
    print("  - rsa_pkcs_pss: RSA PKCS1-PSS (PSS padding, recommended for uboot)")
    print("  - rsa_pkcs:     RSA PKCS#1 v1.5 (standard padding, for other partitions)")
    print("  - eddsa:        Ed25519 (modern, fast, small signatures)")
    print()
    print("Usage: python3 sign_partition.py <file_to_sign> <key_name> <mechanism> <signature_file>")
    print()
    print("Arguments:")
    print("  file_to_sign    - File to sign (e.g., uboot.bin, zImage, rootfs.squashfs)")
    print("  key_name        - HSM key name (default: partition_key)")
    print("  mechanism       - Signing mechanism (required)")
    print("                    Options:")
    print("                      rsa_pkcs_pss  - RSA PKCS1-PSS (for uboot)")
    print("                      rsa_pkcs      - RSA PKCS#1 v1.5 (for other partitions)")
    print("                      eddsa         - Ed25519 (modern, fast)")
    print("  signature_file  - Output signature file path (REQUIRED, no default)")
    print()
    print("Examples:")
    print("  # Sign uboot with PSS padding, save to custom location")
    print("  python3 sign_partition.py uboot.bin partition_key rsa_pkcs_pss /custom/path/signed_uboot.bin")
    print()
    print("  # Sign kernel with standard PKCS#1 v1.5 padding")
    print("  python3 sign_partition.py zImage.crypted partition_key rsa_pkcs /backup/zImage.signed")
    print()
    print("  # Sign with Ed25519 (modern, fast signatures)")
    print("  python3 sign_partition.py firmware.bin ed25519_key eddsa /output/firmware.sig")
    print()
    print("  # Sign with custom key name and custom output")
    print("  python3 sign_partition.py file.bin my_key rsa_pkcs_pss /output/my_file.sig")
    print()
    print("Ed25519 Advantages:")
    print("  - Fast signing and verification")
    print("  - Small 64-byte signatures (vs 256+ bytes for RSA)")
    print("  - Better security guarantees than RSA")
    print("  - No padding parameters needed")
    print("  - Built-in hash function")
    print()
    print("NOTE: Signature file path is MANDATORY. You must specify it as the 4th argument.")
    print("      Ed25519 verification uses Python cryptography library.")
    print()

def get_public_key(server_url, token, key_name, public_key_path):
    """Retrieve public key from HSM"""
    print("1. Retrieving public key from HSM...")

    if os.path.exists(public_key_path):
        print(f"✅ Public key file '{public_key_path}' already exists locally")
        print("   Skipping HSM retrieval (use -f flag to force re-fetch)")
        return True

    print("   No local public key found, retrieving from HSM...")

    try:
        response = requests.get(
            f"{server_url}/api/v1/keys/{key_name}",
            headers={"Authorization": f"Bearer {token}"}
        )

        if response.status_code == 200:
            try:
                data = response.json()
                public_key = data.get("public_key", "")

                if public_key:
                    with open(public_key_path, 'w') as f:
                        f.write(public_key)
                    print(f"✅ Public key saved to {public_key_path}")
                    return True
                else:
                    print("⚠️  Could not retrieve public key (this is normal if key doesn't exist yet)")
                    return True
            except json.JSONDecodeError:
                print("⚠️  Could not parse JSON response")
                return True
        else:
            print(f"⚠️  HTTP {response.status_code} when retrieving public key")
            return True

    except requests.RequestException as e:
        print(f"⚠️  Error retrieving public key: {e}")
        return True

def calculate_file_hash(file_path, hash_algorithm="sha256"):
    """Calculate file hash locally and return binary digest"""
    if hash_algorithm == "sha256":
        hash_obj = hashlib.sha256()
    else:
        raise ValueError(f"Unsupported hash algorithm: {hash_algorithm}")

    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_obj.update(chunk)

    return hash_obj.digest()

def sign_file(server_url, token, file_to_sign, key_name, mechanism, signature_file, hash_alg, salt_len):
    """Sign file using HSM"""
    print("2. Signing file with HSM...")

    try:
        # Different handling for RSA vs EdDSA
        if mechanism.startswith("rsa_"):
            # Calculate hash locally for RSA
            print("   Calculating file hash locally...")
            file_hash = calculate_file_hash(file_to_sign, hash_alg)
            print(f"   Hash calculated: {len(file_hash)} bytes")

            # Upload hash as file content (like EdDSA)
            files = {'file': ('hash', file_hash, 'application/octet-stream')}

            data = {
                'key_name': key_name,
                'mechanism': mechanism,
                'skip_hash_calculation': 'true',  # Skip backend hash calculation
                'hash_algorithm': hash_alg
            }

            # Add PSS-specific parameters
            if mechanism == "rsa_pkcs_pss" and salt_len:
                data['salt_length'] = salt_len

            response = requests.post(
                f"{server_url}/api/v1/sign/file",
                headers={"Authorization": f"Bearer {token}"},
                files=files,
                data=data
            )

        else:  # EdDSA - unchanged, keep original logic
            with open(file_to_sign, 'rb') as f:
                files = {'file': f}

                data = {
                    'key_name': key_name,
                    'mechanism': mechanism,
                    'skip_hash_calculation': 'true'  # Skip backend hash calculation for Ed25519
                }

                # Add EdDSA-specific parameters
                if mechanism == "eddsa":
                    data['curve'] = 'ed25519'

                response = requests.post(
                    f"{server_url}/api/v1/sign/file",
                    headers={"Authorization": f"Bearer {token}"},
                    files=files,
                    data=data
                )

        if response.status_code == 200:
            with open(signature_file, 'wb') as sig_file:
                sig_file.write(response.content)

            signature_size = len(response.content)
            print("✅ File signed successfully!")
            print()
            print("📊 Signature details:")
            print(f"  Output file: {signature_file}")
            print(f"  Signature size: {signature_size} bytes")
            print()
            return True
        else:
            # Delete signature file on error
            if os.path.exists(signature_file):
                os.remove(signature_file)

            print("❌ Signing failed!")
            print(f"HTTP Response Code: {response.status_code}")
            print(f"Response: {response.text}")
            return False

    except requests.RequestException as e:
        if os.path.exists(signature_file):
            os.remove(signature_file)
        print(f"❌ Signing failed! Request error: {e}")
        return False
    except IOError as e:
        print(f"❌ File operation failed: {e}")
        return False

def verify_ed25519_signature(file_to_sign, signature_file, public_key_path):
    """Verify Ed25519 signature using Python cryptography library"""
    try:
        # Read public key
        with open(public_key_path, 'rb') as f:
            public_key_pem = f.read()

        print(f"   Public key PEM: {public_key_pem}")

        # Parse public key using x509 format like Go version
        from cryptography import x509

        # First try parsing as PEM then extract the raw bytes like Go version
        public_key = serialization.load_pem_public_key(
            public_key_pem,
            backend=default_backend()
        )

        if not isinstance(public_key, ed25519.Ed25519PublicKey):
            print("❌ Not an Ed25519 public key")
            return False

        # Get raw public key bytes like Go version's ed25519.PublicKey
        raw_public_key = public_key.public_bytes(
            encoding=serialization.Encoding.Raw,
            format=serialization.PublicFormat.Raw
        )
        print(f"   Public key bytes: {raw_public_key.hex()}")

        # Print public key as C array for easy pasting
        print(f"   Public key as C array:")
        c_array = ', '.join([f'0x{b:02x}' for b in raw_public_key])
        print(f"   uint8_t public_key[{len(raw_public_key)}] = {{ {c_array} }};")

        # Read original file and signature
        with open(file_to_sign, 'rb') as f:
            original_data = f.read()

        with open(signature_file, 'rb') as f:
            signature = f.read()

        print(f"   Original data: {original_data.hex()}")
        print(f"   Signature: {signature.hex()}")

        # Verify signature using cryptography library
        try:
            public_key.verify(signature, original_data)
            print("✅ Ed25519 signature verification PASSED!")
            print("   The signature is valid and matches the file.")
            return True
        except Exception as verify_error:
            print(f"❌ Ed25519 signature verification FAILED: {verify_error}")
            print("   The signature does not match the file.")
            return False

    except Exception as e:
        print(f"❌ Ed25519 signature verification failed: {e}")
        return False

def verify_signature(file_to_sign, signature_file, public_key_path, mechanism):
    """Verify signature using OpenSSL or Python Ed25519 verification"""
    print("3. Verifying signature (mandatory)...")

    if not os.path.exists(public_key_path):
        print("❌ Error: Cannot verify signature - public key not available!")
        print(f"   Expected file: {public_key_path}")
        return False

    # Build verification command based on mechanism
    if mechanism == "eddsa":
        # Use Python Ed25519 verification
        print(f"   Using Python cryptography library for Ed25519 verification")
        return verify_ed25519_signature(file_to_sign, signature_file, public_key_path)
    elif mechanism == "rsa_pkcs_pss":
        verify_cmd = [
            "openssl", "dgst", "-sha256",
            "-sigopt", "rsa_padding_mode:pss",
            "-sigopt", "rsa_pss_saltlen:32",
            "-verify", public_key_path,
            "-signature", signature_file,
            file_to_sign
        ]
    else:  # rsa_pkcs
        verify_cmd = [
            "openssl", "dgst", "-sha256",
            "-verify", public_key_path,
            "-signature", signature_file,
            file_to_sign
        ]

    try:
        result = subprocess.run(verify_cmd, capture_output=True, text=True)

        # OpenSSL verification
        if "Verified OK" in result.stdout:
            print("✅ Signature verification PASSED!")
            print("   The signature is valid and matches the file.")
            return True
        else:
            print("❌ Signature verification FAILED!")
            print("   The signature does not match the file.")
            print()
            print("Debug output:")
            print(result.stdout)
            if result.stderr:
                print("STDERR:", result.stderr)
            return False

    except subprocess.SubprocessError as e:
        print(f"❌ Verification command failed: {e}")
        return False

def display_signature_info(signature_file, verify_cmd):
    """Display signature file information"""
    print("4. Signature file information:")

    try:
        # Use file command to get file type
        result = subprocess.run(["file", signature_file], capture_output=True, text=True)
        print(result.stdout.strip())
    except subprocess.SubprocessError:
        print("   Could not determine file type")

    print()
    print("   First 32 bytes (hex):")

    try:
        with open(signature_file, 'rb') as f:
            data = f.read(32)
            hex_str = ' '.join(f'{b:02x}' for b in data)
            print(f"   {hex_str}")
    except IOError:
        print("   Could not read file")

    print()

def main():
    """Main function"""
    parser = argparse.ArgumentParser(
        description='HSM Partition File Signing Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s uboot.bin partition_key rsa_pkcs_pss /custom/path/signed_uboot.bin
  %(prog)s zImage.crypted partition_key rsa_pkcs /backup/zImage.signed
  %(prog)s file.bin my_key rsa_pkcs_pss /output/my_file.sig
        """
    )

    parser.add_argument('file_to_sign', help='File to sign')
    parser.add_argument('key_name', nargs='?', default=DEFAULT_KEY_NAME, help='HSM key name')
    parser.add_argument('mechanism', choices=['rsa_pkcs_pss', 'rsa_pkcs', 'eddsa'], help='Signing mechanism')
    parser.add_argument('signature_file', help='Output signature file path (REQUIRED)')

    args = parser.parse_args()

    # Validate inputs
    if not os.path.exists(args.file_to_sign):
        print(f"❌ Error: File '{args.file_to_sign}' not found!")
        sys.exit(1)

    # Create directories
    signature_dir = os.path.dirname(args.signature_file)
    os.makedirs(signature_dir, exist_ok=True)
    os.makedirs(PUBLIC_KEY_DIR, exist_ok=True)

    # Set file paths
    public_key_path = os.path.join(PUBLIC_KEY_DIR, f"{args.key_name}.pem")

    # Get file size
    file_size = os.path.getsize(args.file_to_sign)

    print("=== HSM Partition File Signing Tool ===")
    print()
    print("📋 Configuration:")
    print(f"  Mechanism: {args.mechanism} (user-specified)")
    print()
    print("Configuration:")
    print(f"  Server URL: {SERVER_URL}")
    print(f"  Token: [hardcoded] {TOKEN[:20]}...")
    print(f"  File to sign: {args.file_to_sign}")
    print(f"  Signature directory: {signature_dir}/")
    print(f"  Signature output: {args.signature_file}")
    print(f"  Public key directory: {PUBLIC_KEY_DIR}/")
    print(f"  Public key file: {public_key_path}")
    print(f"  Key name: {args.key_name}")
    print(f"  Mechanism: {args.mechanism}")
    print()

    print("📄 File information:")
    print(f"  File: {args.file_to_sign}")
    print(f"  Size: {file_size} bytes")
    print()

    # Set mechanism-specific parameters
    hash_alg = "sha256"
    salt_len = ""

    if args.mechanism == "eddsa":
        print("🔐 Signature mechanism: Ed25519 (EdDSA)")
        print("   - Algorithm: Ed25519")
        print("   - Curve: Ed25519")
        print("   - Signature size: 64 bytes")
        print("   - Built-in hashing")
    elif args.mechanism == "rsa_pkcs_pss":
        salt_len = "32"
        print("🔐 Signature mechanism: RSA PKCS1-PSS")
        print("   - Padding: PSS")
        print("   - Hash: SHA-256")
        print("   - MGF: MGF1-SHA256")
        print("   - Salt length: 32 bytes")
    elif args.mechanism == "rsa_pkcs":
        print("🔐 Signature mechanism: RSA PKCS#1 v1.5")
        print("   - Padding: PKCS#1 v1.5")
        print("   - Hash: SHA-256")

    print()

    # Get public key
    if not get_public_key(SERVER_URL, TOKEN, args.key_name, public_key_path):
        print("⚠️  Continuing with public key retrieval...")

    print()

    # Sign file
    if not sign_file(SERVER_URL, TOKEN, args.file_to_sign, args.key_name,
                    args.mechanism, args.signature_file, hash_alg, salt_len):
        sys.exit(1)

    # Build verification command for display
    if args.mechanism == "eddsa":
        verify_cmd_display = f"verify_ed25519_signature('{args.file_to_sign}', '{args.signature_file}', '{public_key_path}')"
    elif args.mechanism == "rsa_pkcs_pss":
        verify_cmd_display = f"openssl dgst -sha256 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 -verify {public_key_path} -signature {args.signature_file} {args.file_to_sign}"
    else:  # rsa_pkcs
        verify_cmd_display = f"openssl dgst -sha256 -verify {public_key_path} -signature {args.signature_file} {args.file_to_sign}"

    # Verify signature
    if not verify_signature(args.file_to_sign, args.signature_file, public_key_path, args.mechanism):
        sys.exit(1)

    print()

    # Display signature info
    display_signature_info(args.signature_file, verify_cmd_display)

    # Usage instructions
    print("=== Signing Complete ===")
    print()
    print("📝 Summary:")
    print(f"  ✅ File signed with {args.mechanism} successfully")
    print("  ✅ Signature verified and validated")
    print(f"  📂 Signature location: {args.signature_file}")
    print()
    print("🔧 To verify this signature later:")
    print(f"  {verify_cmd_display}")
    print()
    print("🔑 Public key location:")
    print(f"  {public_key_path} (retrieved from HSM)")
    print()
    print("📊 Technical details:")
    if args.mechanism == "eddsa":
        print("  Algorithm: Ed25519")
        print("  Curve: Ed25519")
        print("  Signature size: 64 bytes")
        print("  Hash function: Built-in SHA-512")
    else:
        print(f"  Algorithm: RSA-{DEFAULT_KEY_SIZE}")
        if args.mechanism == "rsa_pkcs_pss":
            print("  Padding: PKCS#1 v1.5 PSS")
            print("  Salt length: 32 bytes")
        else:
            print("  Padding: PKCS#1 v1.5")
        print("  Hash function: SHA-256")
    print(f"  Key name: {args.key_name} (shared for all partitions)")
    print(f"  Signature file: {args.signature_file} (user-specified)")
    print()
    print("✨ This script supports RSA (PKCS#1 v1.5, PKCS1-PSS) and Ed25519 signatures!")
    print("✨ Signature verification is automatically performed after signing!")
    print("✨ Signature file location is fully customizable by user!")
    if args.mechanism == "eddsa":
        print("✨ Ed25519 offers fast performance and small signature sizes!")

if __name__ == "__main__":
    main()