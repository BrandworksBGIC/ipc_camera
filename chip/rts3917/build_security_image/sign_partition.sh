#!/bin/bash

# HSM Partition File Signing Script
# Supports multiple partition types with appropriate signing mechanisms
# All signatures are stored in a unified signatures/ directory

SERVER_URL="http://localhost:8080"

# Hardcoded access token
TOKEN="4d0a780cf562a217d432bbda9fb1837db10d8e5ec5e033f51fac808c36a7e35a"

# Default values
KEY_NAME="partition_key"
DEFAULT_KEY_SIZE=2048
PUBLIC_KEY_DIR="public_key"
# Signature file location must be specified as 4th command line argument (no default)

echo "=== HSM Partition File Signing Tool ==="
echo
echo "Supports the following signing mechanisms:"
echo "  - rsa_pkcs_pss: RSA PKCS1-PSS (PSS padding, recommended for uboot)"
echo "  - rsa_pkcs:     RSA PKCS#1 v1.5 (standard padding, for other partitions)"
echo

# Shared help function
show_help() {
    echo "Usage: $0 <file_to_sign> <key_name> <mechanism> <signature_file>"
    echo
    echo "Arguments:"
    echo "  file_to_sign    - File to sign (e.g., uboot.bin, zImage, rootfs.squashfs)"
    echo "  key_name        - HSM key name (default: partition_key)"
    echo "  mechanism       - Signing mechanism (required)"
    echo "                    Options:"
    echo "                      rsa_pkcs_pss  - RSA PKCS1-PSS (for uboot)"
    echo "                      rsa_pkcs      - RSA PKCS#1 v1.5 (for other partitions)"
    echo "  signature_file  - Output signature file path (REQUIRED, no default)"
    echo
    echo "Examples:"
    echo "  # Sign uboot with PSS padding, save to custom location"
    echo "  $0 uboot.bin partition_key rsa_pkcs_pss /custom/path/signed_uboot.bin"
    echo
    echo "  # Sign kernel with standard PKCS#1 v1.5 padding"
    echo "  $0 zImage.crypted partition_key rsa_pkcs /backup/zImage.signed"
    echo
    echo "  # Sign with custom key name and custom output"
    echo "  $0 file.bin my_key rsa_pkcs_pss /output/my_file.sig"
    echo
    echo "NOTE: Signature file path is MANDATORY. You must specify it as the 4th argument."
    echo
}

# Check if file to sign is provided
if [ -z "$1" ]; then
    show_help
    exit 1
fi

FILE_TO_SIGN=$1
KEY_NAME=${2:-$KEY_NAME}
MECHANISM=$3
SIGNATURE_FILE=$4

# Validate mechanism is provided
if [ -z "$MECHANISM" ]; then
    echo "❌ Error: Mechanism must be specified!"
    echo "   Available mechanisms: rsa_pkcs_pss, rsa_pkcs"
    exit 1
fi

# Validate signature file path is provided (MANDATORY - no default)
if [ -z "$SIGNATURE_FILE" ]; then
    echo "❌ Error: Signature file path must be specified!"
    echo
    show_help
    exit 1
fi

if [ ! -f "$FILE_TO_SIGN" ]; then
    echo "❌ Error: File '$FILE_TO_SIGN' not found!"
    exit 1
fi

# Create parent directory for signature file if it doesn't exist
SIGNATURE_DIR=$(dirname "$SIGNATURE_FILE")
mkdir -p "$SIGNATURE_DIR"

# Create public key directory if it doesn't exist
mkdir -p "$PUBLIC_KEY_DIR"

# Set public key file path
PUBLIC_KEY_PATH="${PUBLIC_KEY_DIR}/${KEY_NAME}.pub"

echo "📋 Configuration:"
echo "  Mechanism: $MECHANISM (user-specified)"
echo
echo "Configuration:"
echo "  Server URL: $SERVER_URL"
echo "  Token: [hardcoded] ${TOKEN:0:20}..."
echo "  File to sign: $FILE_TO_SIGN"
echo "  Signature directory: $SIGNATURE_DIR/"
echo "  Signature output: $SIGNATURE_FILE"
echo "  Public key directory: $PUBLIC_KEY_DIR/"
echo "  Public key file: $PUBLIC_KEY_PATH"
echo "  Key name: $KEY_NAME"
echo "  Mechanism: $MECHANISM"
echo

# Get file size
FILE_SIZE=$(wc -c < "$FILE_TO_SIGN")
echo "📄 File information:"
echo "  File: $FILE_TO_SIGN"
echo "  Size: $FILE_SIZE bytes"
echo

# Set mechanism-specific parameters
case "$MECHANISM" in
    rsa_pkcs_pss)
        HASH_ALG="sha256"
        SALT_LEN="32"
        echo "🔐 Signature mechanism: RSA PKCS1-PSS"
        echo "   - Padding: PSS"
        echo "   - Hash: SHA-256"
        echo "   - MGF: MGF1-SHA256"
        echo "   - Salt length: 32 bytes"
        ;;
    rsa_pkcs)
        HASH_ALG="sha256"
        SALT_LEN=""
        echo "🔐 Signature mechanism: RSA PKCS#1 v1.5"
        echo "   - Padding: PKCS#1 v1.5"
        echo "   - Hash: SHA-256"
        ;;
    *)
        echo "❌ Error: Unsupported mechanism: $MECHANISM"
        echo "Supported mechanisms: rsa_pkcs_pss, rsa_pkcs"
        exit 1
        ;;
esac
echo

# Verify token
echo "1. Verifying access token..."
TOKEN_CHECK=$(curl -s -X GET "$SERVER_URL/api/v1/keys/$KEY_NAME" \
    -H "Authorization: Bearer $TOKEN" 2>&1)

if echo "$TOKEN_CHECK" | grep -q "error\|unauthorized"; then
    echo "❌ Authentication failed! Please check your access token."
    echo "Response: $TOKEN_CHECK"
    exit 1
fi

echo "✅ Authentication successful"
echo

# Get public key for verification (skip if already exists locally)
echo "2. Retrieving public key from HSM..."

if [ -f "$PUBLIC_KEY_PATH" ]; then
    echo "✅ Public key file '$PUBLIC_KEY_PATH' already exists locally"
    echo "   Skipping HSM retrieval (use -f flag to force re-fetch)"
else
    echo "   No local public key found, retrieving from HSM..."
    PUBLIC_KEY_RESPONSE=$(curl -s -X GET "$SERVER_URL/api/v1/keys/$KEY_NAME" \
        -H "Authorization: Bearer $TOKEN")

    PUBLIC_KEY=$(echo "$PUBLIC_KEY_RESPONSE" | grep -o '"public_key":"[^"]*' | cut -d'"' -f4 | sed 's/\\n/\n/g' | sed 's/\\"/"/g')

    if [ -n "$PUBLIC_KEY" ]; then
        echo "$PUBLIC_KEY" > "$PUBLIC_KEY_PATH"
        echo "✅ Public key saved to $PUBLIC_KEY_PATH"
    else
        echo "⚠️  Could not retrieve public key (this is normal if key doesn't exist yet)"
    fi
fi
echo

# Sign the file
echo "3. Signing file with HSM..."

# Build curl command based on mechanism
CURL_CMD="curl -s -X POST \"$SERVER_URL/api/v1/sign/file\" \
    -H \"Authorization: Bearer $TOKEN\" \
    -F \"key_name=$KEY_NAME\" \
    -F \"mechanism=$MECHANISM\" \
    -F \"skip_hash_calculation=false\" \
    -F \"file=@$FILE_TO_SIGN\" \
    --output \"$SIGNATURE_FILE\" \
    -w \"\\nHTTP_CODE:%{http_code}\""

# Add mechanism-specific parameters
if [ "$MECHANISM" = "rsa_pkcs_pss" ]; then
    CURL_CMD="$CURL_CMD -F \"hash_algorithm=$HASH_ALG\" -F \"salt_length=$SALT_LEN\""
elif [ "$MECHANISM" = "rsa_pkcs" ]; then
    CURL_CMD="$CURL_CMD -F \"hash_algorithm=$HASH_ALG\""
fi

SIGN_RESPONSE=$(eval $CURL_CMD)

HTTP_CODE=$(echo "$SIGN_RESPONSE" | grep "HTTP_CODE:" | cut -d':' -f2)
SIGN_RESPONSE=$(echo "$SIGN_RESPONSE" | sed '/HTTP_CODE:/d')

if [ "$HTTP_CODE" != "200" ]; then
    # Delete signature file on error
    rm -f "$SIGNATURE_FILE"
    echo "❌ Signing failed!"
    echo "HTTP Response Code: $HTTP_CODE"
    echo "Response: $SIGN_RESPONSE"
    exit 1
fi

if [ ! -f "$SIGNATURE_FILE" ] || [ ! -s "$SIGNATURE_FILE" ]; then
    echo "❌ Error: Signature file is empty or was not created!"
    exit 1
fi

SIGNATURE_SIZE=$(wc -c < "$SIGNATURE_FILE")
echo "✅ File signed successfully!"
echo
echo "📊 Signature details:"
echo "  Output file: $SIGNATURE_FILE"
echo "  Signature size: $SIGNATURE_SIZE bytes"
echo

# Verify the signature using OpenSSL (MANDATORY)
echo "4. Verifying signature (mandatory)..."

if [ ! -f "$PUBLIC_KEY_PATH" ]; then
    echo "❌ Error: Cannot verify signature - public key not available!"
    echo "   Expected file: $PUBLIC_KEY_PATH"
    exit 1
fi

# Perform verification based on mechanism
VERIFY_CMD=""
case "$MECHANISM" in
    rsa_pkcs_pss)
        VERIFY_CMD="openssl dgst -sha256 -sigopt rsa_padding_mode:pss -sigopt rsa_pss_saltlen:32 -verify $PUBLIC_KEY_PATH -signature $SIGNATURE_FILE $FILE_TO_SIGN"
        ;;
    rsa_pkcs)
        VERIFY_CMD="openssl dgst -sha256 -verify $PUBLIC_KEY_PATH -signature $SIGNATURE_FILE $FILE_TO_SIGN"
        ;;
esac

$VERIFY_CMD 2>&1 | grep -q "Verified OK"
VERIFY_RESULT=$?

if [ $VERIFY_RESULT -eq 0 ]; then
    echo "✅ Signature verification PASSED!"
    echo "   The signature is valid and matches the file."
else
    echo "❌ Signature verification FAILED!"
    echo "   The signature does not match the file."
    echo
    echo "Debug output:"
    $VERIFY_CMD
    exit 1
fi
echo

# Display signature file information
echo "5. Signature file information:"
file "$SIGNATURE_FILE"
echo
echo "   First 32 bytes (hex):"
hexdump -C "$SIGNATURE_FILE" | head -4 | sed 's/^/   /'
echo

# Usage instructions
echo "=== Signing Complete ==="
echo
echo "📝 Summary:"
echo "  ✅ File signed with $MECHANISM successfully"
echo "  ✅ Signature verified and validated"
echo "  📂 Signature location: $SIGNATURE_FILE"
echo
echo "🔧 To verify this signature later:"
echo "  $VERIFY_CMD"
echo
echo "🔑 Public key location:"
echo "  $PUBLIC_KEY_PATH (retrieved from HSM)"
echo
echo "📊 Technical details:"
echo "  Algorithm: RSA-$DEFAULT_KEY_SIZE"
if [ "$MECHANISM" = "rsa_pkcs_pss" ]; then
    echo "  Padding: PKCS#1 v1.5 PSS"
    echo "  Salt length: 32 bytes"
else
    echo "  Padding: PKCS#1 v1.5"
fi
echo "  Hash function: SHA-256"
echo "  Key name: $KEY_NAME (shared for all partitions)"
echo "  Signature file: $SIGNATURE_FILE (user-specified)"
echo
echo "✨ This script supports both uboot (RSA-PSS) and other partitions (RSA-PKCS)!"
echo "✨ Signature verification is automatically performed after signing!"
echo "✨ Signature file location is fully customizable by user!"

exit 0
