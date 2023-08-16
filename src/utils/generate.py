# coding=utf-8

import os
import binascii
from elftools.elf.elffile import ELFFile

DIRNAME = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.abspath(os.path.join(DIRNAME, "../../build"))
KERNELBIN = os.path.join(BUILD, 'kernel.bin')
SYSTEMBIN = os.path.join(BUILD, 'system.bin')


def main():
    start = 0x8000
    size = os.path.getsize(SYSTEMBIN) - start

    with open(SYSTEMBIN, 'rb') as file:
        data = file.read()
        chksum = binascii.crc32(data[start:])
        print(f"system.bin chksum: {chksum} size {size}")

    with open(KERNELBIN, 'rb+') as file:
        elf = ELFFile(file)
        section = elf.get_section_by_name(".onix.magic")
        offset = section["sh_offset"]
        file.seek(offset)
        magic = int.from_bytes(file.read(4), "little")
        if magic != 0x20220205:
            print("magic check error")
            return

        section = elf.get_section_by_name(".onix.size")
        offset = section["sh_offset"]
        file.seek(offset)
        file.write(size.to_bytes(4, "little"))
        print(f"write size {size} offset {offset:x} success")

        section = elf.get_section_by_name(".onix.chksum")
        offset = section["sh_offset"]
        file.seek(offset)
        file.write(chksum.to_bytes(4, "little"))
        print(f"write size {chksum:#x} offset {offset:x} success:")


if __name__ == '__main__':
    main()
