#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import os
import sys

def file_to_hex_grouped(file_path, group_size=4):
    with open(file_path, 'rb') as file:
        content = file.read()
        hex_content = content.hex()
        grouped_hex = []
        for i in range(0, len(hex_content), group_size * 2):
            group = hex_content[i:i + group_size * 2]
            grouped_hex.append(f"0x{group}")
        return ' '.join(grouped_hex)

if __name__ == "__main__":
    file_path = sys.argv[1]
    hex_representation = file_to_hex_grouped(file_path)
    print(hex_representation)
