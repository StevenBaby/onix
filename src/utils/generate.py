# coding=utf-8

import os
import binascii
import shutil
try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    print('''
        import elftools failure.
        You may need to install elftools packet using either
            pip install pyelftools
        or in Archlinux
            sudo pacman -S python-pyelftools
    ''')
    exit(-1)

DIRNAME = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.abspath(os.path.join(DIRNAME, "../../build"))
KERNELRAWBIN = os.path.join(BUILD, 'kernel.raw.bin')
SYSTEMRAWBIN = os.path.join(BUILD, 'system.raw.bin')
KERNELBIN = os.path.join(BUILD, 'kernel.bin')


def main():
    start = 0x8000
    size = os.path.getsize(SYSTEMRAWBIN) - start

    with open(SYSTEMRAWBIN, 'rb') as file:
        data = file.read()
        chksum = binascii.crc32(data[start:])
        print(f"system.bin chksum: {chksum} size {size}")

    shutil.copy(KERNELRAWBIN, KERNELBIN)

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
