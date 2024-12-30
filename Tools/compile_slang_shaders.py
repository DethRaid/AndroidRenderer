'''Compiles slang shaders for Vulkan 1.3 SPIR-V

Basic usage: compile_slang_shaders.py <input_directory> <output_directory>
'''


import os
import subprocess
import sys
from pathlib import Path


def compile_shaders_in_path(path, root_dir, output_dir):
    for child_path in path.iterdir():
        if child_path.is_dir():
            compile_shaders_in_path(child_path, root_dir, output_dir)

        elif child_path.suffix == '.slang':
            relative_file_path = child_path.relative_to(root_dir)
            print(f"{relative_file_path}")

            output_file = output_dir / relative_file_path
            output_parent = output_file.parent
            output_parent.mkdir(parents=True, exist_ok=True)

            command = f"{slang_exe} {child_path} -profile glsl_460 -target spirv -entry main -o {output_file}"
            print(f"Compile command: {command}")
            subprocess.run([slang_exe, child_path, '-profile', 'glsl_460', '-target', 'spirv', '-entry', 'main', '-o', output_file, '-I', root_dir, '-I', root_dir.parent])


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: compile_slang_shaders.py <input_directory> <output_directory>")
        exit()

    input_dir = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    print(f"Compilling shaders from {input_dir} to {output_dir}")

    vulkan_sdk_dir = os.environ["VULKAN_SDK"]
    slang_exe = Path(vulkan_sdk_dir) / "Bin" / "slangc.exe"

    print(f"Using Slang executable {slang_exe}")

    # .\slangc.exe hello-world.slang -profile glsl_460 -target spirv -entry main -o hello-world.spv

    # Iterate over the input directory. For every .slang file, compile it to the output directory. Create a folder structure mirroring the folder structure in the input directory

    compile_shaders_in_path(input_dir, input_dir, output_dir)

