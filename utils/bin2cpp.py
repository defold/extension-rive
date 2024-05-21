#!/usr/bin/env python3

import sys, os

def generate_source(input, h_out_file, cpp_out_file):
    with open(input, 'rb') as f:
        data = f.read()

    data_is_binary = type(data) == bytes
    symbol = os.path.basename(input).upper().replace('.', '_').replace('-', '_').replace('@', 'at')

    with open(cpp_out_file, 'w', encoding='utf-8') as f:
        f.write(f'// Generated, do not edit!\n')
        f.write(f'#include <stdint.h>\n')
        f.write(f'uint8_t {symbol}[] = {{\n    ')

        tmp = ''
        for i,x in enumerate(data):
            if data_is_binary:
                tmp += hex(x) + ', '
            else:
                tmp += hex(ord(x)) + ', '
            if i > 0 and i % 16 == 0:
                tmp += '\n    '

        f.write(tmp)
        f.write(f'\n}};\n')
        f.write(f'uint32_t {symbol}_SIZE = sizeof({symbol});\n')

    with open(h_out_file , 'w', encoding='utf-8') as f:
        f.write(f'// Generated, do not edit!\n')
        f.write(f'#include <stdint.h>\n')
        f.write(f'extern "C" uint8_t {symbol}[];\n')
        f.write(f'extern "C" uint32_t {symbol}_SIZE;\n')


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: bin2cpp.py <input> <output_header> <output_source>")
        sys.exit(1)

    generate_source(sys.argv[1], sys.argv[2], sys.argv[3])
