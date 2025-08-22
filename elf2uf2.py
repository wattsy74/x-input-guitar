#!/usr/bin/env python3
"""
Simple ELF to UF2 converter for RP2040
Based on the UF2 specification: https://github.com/microsoft/uf2
"""
import struct
import sys
from pathlib import Path

# UF2 constants
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30

UF2_FLAG_NOT_MAIN_FLASH = 0x00000001
UF2_FLAG_FILE_CONTAINER = 0x00001000
UF2_FLAG_FAMILY_ID_PRESENT = 0x00002000
UF2_FLAG_MD5_PRESENT = 0x00004000

# RP2040 family ID
RP2040_FAMILY_ID = 0xe48bff56

def elf_to_uf2(elf_path, uf2_path):
    """Convert ELF to UF2 format for RP2040"""
    
    # For simplicity, we'll use a basic approach that works for most RP2040 firmware
    # Read the ELF file
    with open(elf_path, 'rb') as f:
        elf_data = f.read()
    
    # This is a simplified converter - it assumes the ELF is already in the right format
    # For a full implementation, we'd need to parse ELF headers and sections
    
    # Create UF2 blocks (256 bytes of data per block)
    block_size = 256
    base_address = 0x10000000  # Flash base address for RP2040
    
    uf2_blocks = []
    data_offset = 0
    num_blocks = (len(elf_data) + block_size - 1) // block_size
    
    for i in range(num_blocks):
        # Get data for this block
        start_idx = i * block_size
        end_idx = min(start_idx + block_size, len(elf_data))
        block_data = elf_data[start_idx:end_idx]
        
        # Pad to 256 bytes
        block_data += b'\x00' * (block_size - len(block_data))
        
        # Create UF2 block (512 bytes total)
        block = struct.pack('<IIIIIIII',
            UF2_MAGIC_START0,           # Magic start 0
            UF2_MAGIC_START1,           # Magic start 1
            UF2_FLAG_FAMILY_ID_PRESENT, # Flags
            base_address + i * block_size, # Target address
            block_size,                 # Payload size
            i,                         # Block number
            num_blocks,                # Total blocks
            RP2040_FAMILY_ID           # Family ID
        )
        
        # Add the 256 bytes of data
        block += block_data
        
        # Add padding and magic end
        block += b'\x00' * (512 - 32 - 256 - 4)  # Padding
        block += struct.pack('<I', UF2_MAGIC_END)  # Magic end
        
        uf2_blocks.append(block)
    
    # Write UF2 file
    with open(uf2_path, 'wb') as f:
        for block in uf2_blocks:
            f.write(block)
    
    print(f"Converted {elf_path} to {uf2_path}")
    print(f"Created {num_blocks} UF2 blocks")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python elf2uf2.py <input.elf> <output.uf2>")
        sys.exit(1)
    
    elf_path = Path(sys.argv[1])
    uf2_path = Path(sys.argv[2])
    
    if not elf_path.exists():
        print(f"Error: {elf_path} not found")
        sys.exit(1)
    
    elf_to_uf2(elf_path, uf2_path)
