import sys
import struct
import zlib

FIRMWARE_MAGIC = 0x41555241
VALID_STATUS = 0x12345678
PT_MAGIC = 0x50415254

def build_image(bootloader_path, app_path, output_path):
    with open(app_path, 'rb') as f:
        app_data = f.read()

    with open(bootloader_path, 'rb') as f:
        bootloader_data = f.read()

    # 1. Bootloader (28KB)
    if len(bootloader_data) > 28672:
        print("ERROR: Bootloader is larger than 28KB!")
        sys.exit(1)
    
    padded_bootloader = bootloader_data.ljust(28672, b'\xFF')

    # 2. Partition Table (4KB)
    # magic(4), bootloader(8), part_a(8), part_b(8), vfs(8), crc(4)
    pt_data = struct.pack('<I II II II II',
        PT_MAGIC,
        0x00000000, 0x00007000, # Bootloader
        0x00008000, 0x00018000, # PART_A
        0x00020000, 0x00018000, # PART_B
        0x00038000, 0x00008000  # VFS
    )
    pt_crc = zlib.crc32(pt_data) & 0xFFFFFFFF
    pt_data += struct.pack('<I', pt_crc)
    padded_pt = pt_data.ljust(4096, b'\xFF')

    # 3. Active Image Header (128 Bytes)
    version = 1
    image_size = len(app_data)
    
    # Mock Ed25519 signature
    signature = bytes([0xED] * 64)
    padding = bytes([0x00] * 48)

    header = struct.pack('<I I I I 64s 48s', 
        FIRMWARE_MAGIC, 
        version, 
        image_size, 
        VALID_STATUS, 
        signature,
        padding
    )

    # Note: Header is exactly 128 bytes, no extra padding needed for linker offset 0x8080.
    
    # Concatenate: Bootloader (28K) + PT (4K) + Header (128B) + App
    flash_data = padded_bootloader + padded_pt + header + app_data

    # Pad out the rest of PART_A if necessary, but QEMU doesn't strictly need it.
    
    with open(output_path, 'wb') as f:
        f.write(flash_data)
    
    print(f"Success! Generated {output_path} (Size: {len(flash_data)} bytes)")
    print(f"App size: {image_size} bytes, Ed25519 Mock Signature Embedded.")

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: python build_image.py <bootloader.bin> <auroraOS.bin> <flash.bin>")
        sys.exit(1)
    build_image(sys.argv[1], sys.argv[2], sys.argv[3])
