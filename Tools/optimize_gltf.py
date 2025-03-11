'''Python script to optimize glTF files

Performs the following optimizations:
- Convert textures to KTX2 textures with UASTC compression
'''

import os
import subprocess
import sys


def get_files_to_compress(directory):
    files_to_compress = []

    for subdir, dirs, files in os.walk(directory):
        for file in files:
            filename = os.path.join(subdir, file)
            if filename.endswith('.compressed.gltf') or filename.endswith('.compressed.glb'):
                # Skip! We'll check if we need to re-compress this file later
                continue

            if filename.endswith('.gltf') or filename.endswith('.glb'): 
                # The file is a gltf file - but not a compressed one. Check if there's a .compressed file, and if this
                # file is newer
                splitted = os.path.splitext(filename)
                compressed_file_name = f"{splitted[0]}.compressed.glb"

                if not os.path.isfile(compressed_file_name):
                    files_to_compress.append((filename, compressed_file_name))

                elif os.path.getmtime(compressed_file_name) < os.path.getmtime(filename):
                    files_to_compress.append((filename, compressed_file_name))

    return files_to_compress


if __name__ == '__main__':
    data_directory = sys.argv[1]

    files_to_compress = get_files_to_compress(data_directory)

    print(f"Files to compress: {len(files_to_compress)}")

    if len(files_to_compress) == 0:
        exit()

    gltfpack_command = 'gltfpack.exe -i {} -o {} -tu -tj 24 -noq'

    processes = []

    for file, compressed_file in files_to_compress:
        command = gltfpack_command.format(file, compressed_file)
        print(f"Compression command: {command}")
        process = subprocess.Popen(command)
        processes.append(process)

    print('Compression processes started')

    for process in processes:
        process.wait()
    
    print('Compression processes finished')
