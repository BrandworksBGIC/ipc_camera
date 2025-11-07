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
import tempfile
import time
import urllib3

# Server configuration for dual HTTP/HTTPS setup with environment variable support
def get_env_config():
    """Get configuration from environment variables with fallbacks"""
    return {
        'server_ip': os.getenv('HSM_SERVER_IP', '127.0.0.1'),
        'http_port': os.getenv('HSM_HTTP_PORT', '8080'),
        'https_port': os.getenv('HSM_HTTPS_PORT', '8443'),
        'token': os.getenv('HSM_TOKEN', '4d0a780cf562a217d432bbda9fb1837db10d8e5ec5e033f51fac808c36a7e35a')
    }

# Get configuration
config = get_env_config()
SERVER_IP = config['server_ip']
HTTP_PORT = config['http_port']
HTTPS_PORT = config['https_port']
TOKEN = config['token']

HTTP_SERVER_URL = f"http://{SERVER_IP}:{HTTP_PORT}"
HTTPS_SERVER_URL = f"https://{SERVER_IP}:{HTTPS_PORT}"
CERT_DOWNLOAD_URL = f"http://{SERVER_IP}:{HTTP_PORT}/api/v1/cert/download"
DEFAULT_KEY_NAME = "partition_key"
DEFAULT_KEY_SIZE = 2048
PUBLIC_KEY_DIR = "public_key"

# Certificate caching
CERT_CACHE_DIR = os.path.expanduser("~/.openhsm/cert")
CERT_FILE = os.path.join(CERT_CACHE_DIR, "openhsm.crt")

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

def download_certificate():
    """Download server certificate via HTTP"""
    print("🔐 Downloading server certificate...")

    # Create certificate cache directory
    os.makedirs(CERT_CACHE_DIR, exist_ok=True)

    try:
        response = requests.get(CERT_DOWNLOAD_URL, timeout=30)
        if response.status_code == 200:
            with open(CERT_FILE, 'w', encoding='utf-8') as f:
                f.write(response.text)
            print(f"✅ Certificate downloaded and cached: {CERT_FILE}")
            return True
        else:
            print(f"❌ Failed to download certificate. HTTP {response.status_code}")
            return False
    except requests.RequestException as e:
        print(f"❌ Certificate download failed: {e}")
        return False

def ensure_certificate():
    """Ensure certificate is available, download if necessary"""
    if os.path.exists(CERT_FILE):
        # Check if certificate is not too old (optional)
        try:
            stat = os.stat(CERT_FILE)
            file_age = time.time() - stat.st_mtime
            if file_age < 86400:  # Less than 1 day old
                print(f"✅ Using cached certificate: {CERT_FILE}")
                return True
            else:
                print(f"⚠️  Certificate is old ({file_age/3600:.1f}h), refreshing...")
        except OSError:
            pass

    # Certificate doesn't exist or is old, download it
    return download_certificate()

def setup_https_session():
    """Setup HTTPS session with certificate verification"""
    if not ensure_certificate():
        print("❌ Failed to setup HTTPS session - no certificate available")
        return None

    try:
        session = requests.Session()
        session.verify = CERT_FILE
        return session
    except Exception as e:
        print(f"❌ Failed to setup HTTPS session: {e}")
        return None

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
    print("Environment Variables:")
    print("  HSM_SERVER_IP   - HSM server IP address (default: 127.0.0.1)")
    print("  HSM_HTTP_PORT   - HSM HTTP port (default: 8080)")
    print("  HSM_HTTPS_PORT  - HSM HTTPS port (default: 8443)")
    print("  HSM_TOKEN       - HSM authentication token (default: hardcoded)")
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
    print("  # Use environment variables for configuration")
    print("  export HSM_SERVER_IP=192.168.1.100")
    print("  export HSM_TOKEN=your_token_here")
    print("  python3 sign_partition.py uboot.bin partition_key rsa_pkcs_pss signed_uboot.bin")
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
    """Retrieve public key from HSM using HTTPS"""
    if os.path.exists(public_key_path):
        print(f"✅ Public key already exists: {public_key_path}")
        return True

    print("🔑 Retrieving public key from HSM...")

    # Setup HTTPS session with certificate verification
    session = setup_https_session()
    if not session:
        print("❌ Failed to setup HTTPS session for public key retrieval")
        return False

    try:
        response = session.get(
            f"{HTTPS_SERVER_URL}/api/v1/keys/{key_name}",
            headers={"Authorization": f"Bearer {token}"},
            timeout=30
        )

        if response.status_code == 200:
            try:
                data = response.json()
                public_key = data.get("public_key", "")

                if public_key:
                    with open(public_key_path, 'w') as f:
                        f.write(public_key)
                    print(f"✅ Public key saved: {public_key_path}")
                    return True
                else:
                    print("⚠️  No public key found (key may not exist yet)")
                    return True
            except json.JSONDecodeError:
                print("⚠️  Invalid JSON response")
                return True
        else:
            print(f"⚠️  HTTPS {response.status_code}")
            return False

    except requests.RequestException as e:
        print(f"⚠️  HTTPS request failed: {e}")
        return False

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
    """Sign file using HSM via HTTPS"""
    print("🔐 Signing file...")

    # Setup HTTPS session with certificate verification
    session = setup_https_session()
    if not session:
        print("❌ Failed to setup HTTPS session for signing")
        return False

    try:
        # Different handling for RSA vs EdDSA
        if mechanism.startswith("rsa_"):
            # Calculate hash locally for RSA
            file_hash = calculate_file_hash(file_to_sign, hash_alg)
            files = {'file': ('hash', file_hash, 'application/octet-stream')}

            data = {
                'key_name': key_name,
                'mechanism': mechanism,
                'skip_hash_calculation': 'true',
                'hash_algorithm': hash_alg
            }

            # Add PSS-specific parameters
            if mechanism == "rsa_pkcs_pss" and salt_len:
                data['salt_length'] = salt_len

            response = session.post(
                f"{HTTPS_SERVER_URL}/api/v1/sign/file",
                headers={"Authorization": f"Bearer {token}"},
                files=files,
                data=data,
                timeout=60
            )

        else:  # EdDSA
            with open(file_to_sign, 'rb') as f:
                files = {'file': f}

                data = {
                    'key_name': key_name,
                    'mechanism': mechanism,
                    'skip_hash_calculation': 'true'
                }

                if mechanism == "eddsa":
                    data['curve'] = 'ed25519'

                response = session.post(
                    f"{HTTPS_SERVER_URL}/api/v1/sign/file",
                    headers={"Authorization": f"Bearer {token}"},
                    files=files,
                    data=data,
                    timeout=60
                )

        if response.status_code == 200:
            with open(signature_file, 'wb') as sig_file:
                sig_file.write(response.content)

            signature_size = len(response.content)
            print(f"✅ Signed successfully! Size: {signature_size} bytes")
            return True
        else:
            if os.path.exists(signature_file):
                os.remove(signature_file)
            print(f"❌ Signing failed! HTTPS {response.status_code}")
            return False

    except requests.RequestException as e:
        if os.path.exists(signature_file):
            os.remove(signature_file)
        print(f"❌ HTTPS request failed: {e}")
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

        # Parse public key
        public_key = serialization.load_pem_public_key(
            public_key_pem,
            backend=default_backend()
        )

        if not isinstance(public_key, ed25519.Ed25519PublicKey):
            print("❌ Not an Ed25519 public key")
            return False

        # Get raw public key bytes
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

        # Verify signature using cryptography library
        try:
            public_key.verify(signature, original_data)
            print("✅ Ed25519 signature verified successfully")
            return True
        except Exception as verify_error:
            print(f"❌ Ed25519 verification failed: {verify_error}")
            return False

    except Exception as e:
        print(f"❌ Ed25519 verification error: {e}")
        return False

def verify_signature(file_to_sign, signature_file, public_key_path, mechanism):
    """Verify signature using OpenSSL or Python Ed25519 verification"""
    print("🔍 Verifying signature...")

    if not os.path.exists(public_key_path):
        print(f"❌ Public key not found: {public_key_path}")
        return False

    # Build verification command based on mechanism
    if mechanism == "eddsa":
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

        if "Verified OK" in result.stdout:
            print("✅ Signature verified successfully")
            return True
        else:
            print("❌ Signature verification failed")
            if result.stderr:
                print(f"Error: {result.stderr.strip()}")
            return False

    except subprocess.SubprocessError as e:
        print(f"❌ Verification command failed: {e}")
        return False

def display_signature_info(signature_file, verify_cmd):
    """Display signature file information"""
    try:
        with open(signature_file, 'rb') as f:
            data = f.read(32)
            hex_str = ' '.join(f'{b:02x}' for b in data)
            print(f"📄 Signature preview: {hex_str}")
    except IOError:
        print("⚠️  Could not read signature file")

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

    print("=== HSM Partition Signing Tool ===")
    print(f"📁 File: {args.file_to_sign} ({file_size} bytes)")
    print(f"🔧 Mechanism: {args.mechanism}")
    print(f"🔑 Key: {args.key_name}")
    print()

    # Set mechanism-specific parameters
    hash_alg = "sha256"
    salt_len = ""

    if args.mechanism == "eddsa":
        print("ℹ️  Ed25519: 64-byte signatures, built-in hashing")
    elif args.mechanism == "rsa_pkcs_pss":
        salt_len = "32"
        print("ℹ️  RSA-PSS: SHA-256, 32-byte salt")
    elif args.mechanism == "rsa_pkcs":
        print("ℹ️  RSA-PKCS#1: SHA-256")

    # Get public key
    if not get_public_key(None, TOKEN, args.key_name, public_key_path):
        print("⚠️  Continuing with public key retrieval...")
        # Don't exit on certificate failure, as the function may have cached the public key

    print()

    # Sign file
    if not sign_file(None, TOKEN, args.file_to_sign, args.key_name,
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

    print("✅ Signing complete!")
    print(f"📁 Output: {args.signature_file}")
    print(f"🔑 Public key: {public_key_path}")
    if args.mechanism == "eddsa":
        print("💡 Fast Ed25519 verification available")

if __name__ == "__main__":
    main()