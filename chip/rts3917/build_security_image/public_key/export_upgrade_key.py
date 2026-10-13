#!/usr/bin/env python3
import base64
import sys
from pathlib import Path


if len(sys.argv) != 3:
    sys.exit("Usage: export_upgrade_key.py public_key.pem public_key.bin")

try:
    lines = Path(sys.argv[1]).read_text(encoding="ascii").strip().splitlines()
    if (len(lines) < 3 or lines[0] != "-----BEGIN PUBLIC KEY-----"
            or lines[-1] != "-----END PUBLIC KEY-----"):
        raise ValueError("invalid public key PEM")
    der = base64.b64decode("".join(lines[1:-1]), validate=True)
    # Ed25519 SubjectPublicKeyInfo contains a fixed header and a 32-byte key.
    if len(der) != 44 or der[:12] != bytes.fromhex("302a300506032b6570032100"):
        raise ValueError("expected an Ed25519 public key")
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.chmod(0o644)
    output.write_bytes(der[12:])
    output.chmod(0o444)
except (OSError, ValueError) as error:
    sys.exit("Failed to export upgrade public key: " + str(error))
