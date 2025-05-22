#!/usr/bin/env python3
import os
import sys

def symbol_name_sanitize(str):
    return str.replace('/', '_').replace('.', '_').replace('-', '_')

def symbol_name(component_name, path):
    return "_binary_components_" + symbol_name_sanitize(component_name) + "_" + symbol_name_sanitize(path)

def content_type(filename):
    ext = filename.rsplit('.', 1)[-1].lower()
    return {
        'html': 'text/html',
        'htm': 'text/html',
        'css': 'text/css',
        'js': 'application/javascript',
        'ico': 'image/x-icon',
        'png': 'image/png',
        'jpg': 'image/jpeg',
        'jpeg': 'image/jpeg',
        'gif': 'image/gif',
        'txt': 'text/plain',
    }.get(ext, 'application/octet-stream')


def write_header(file):
    file.write('// This file is auto-generated. Do not edit! //\n')
    file.write('\n')


if __name__ == '__main__':
    component_name = sys.argv[1] if len(sys.argv) > 1 else 'app_files'
    web_dir = sys.argv[2] if len(sys.argv) > 2 else 'web/'
    header_path = sys.argv[3] if len(sys.argv) > 3 else 'app_files.h'
    source_path = sys.argv[4] if len(sys.argv) > 4 else 'app_files.c'

    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if(not os.path.isabs(web_dir)):
        web_dir = os.path.join(script_dir, web_dir)
    if(not os.path.isabs(header_path)):
        header_path = os.path.join(script_dir, header_path)
    if(not os.path.isabs(source_path)):
        source_path = os.path.join(script_dir, source_path)
    
    entries = []
    externs = []
    for root, dirs, files in os.walk(web_dir):
        for fname in files:
            rel_path = os.path.relpath(os.path.join(root, fname), os.path.dirname(web_dir))
            uri = '/' + os.path.relpath(os.path.join(root, fname), web_dir).replace('\\', '/')
            sym_base = symbol_name(component_name, rel_path)
            externs.append(f'extern const uint8_t {sym_base}_start[];\nextern const uint8_t {sym_base}_end[];\n')
            entries.append(f'    {{ "{uri}", {sym_base}_start, {sym_base}_end, "{content_type(fname)}" }},\n')

    with open(header_path, 'w') as f:
        f.write('#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n')
        for ext in externs:
            f.write(ext)
        f.write('\ntypedef struct {\n    const char *uri;\n    const uint8_t *start;\n    const uint8_t *end;\n    const char *content_type;\n} app_file_t;\n\n')
        f.write('extern const app_file_t app_files[];\n')
        f.write('extern const size_t app_files_count;\n')

    with open(source_path, 'w') as f:
        f.write('#include "app_files.h"\n#include <stddef.h>\n\n')
        f.write('const app_file_t app_files[] = {\n')
        for entry in entries:
            f.write(entry)
        f.write('};\n')
        f.write('const size_t app_files_count = sizeof(app_files)/sizeof(app_files[0]);\n')
    print(f'Generated {len(entries)} symbols from {web_dir} into {header_path} and {source_path}.')
    print('Done.')
