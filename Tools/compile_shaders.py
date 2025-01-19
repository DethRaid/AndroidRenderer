'''Compiles slang shaders for Vulkan 1.3 SPIR-V

Basic usage: compile_slang_shaders.py <input_directory> <output_directory>
'''


import os
import subprocess
import sys
from pathlib import Path


def compile_slang_shader(input_file, output_file, include_directories):
    command = [slang_exe, input_file, '-profile', 'glsl_460', '-target', 'spirv', '-entry', 'main', '-g', '-o', output_file]
    for dir in include_directories:
        command.append('-I')
        command.append(dir)

    # use  -output-includes to get a list of included files, de-duplicate that to get a dependency list, recompile the shader if any of its dependencies have changed

    print(f"Compiling {input_file} as Slang")
    print(f"output_file={output_file}")
    subprocess.run(command)


def compile_glsl_shader(input_file, output_file, include_directories):
    command = [glslang_exe, "--target-env", "vulkan1.3", "-V", "-g", "-o", "-Od"]
    for dir in include_directories:
        command.append(f"-I{dir}")

    command.append(input_file)
    command.append("-o")
    command.append(output_file)

    print(f"Compiling {input_file} as GLSL")
    print(f"output_file={output_file}")
    subprocess.run(command)


def compile_shaders_in_path(path, root_dir, output_dir):
    for child_path in path.iterdir():
        if child_path.is_dir():
            compile_shaders_in_path(child_path, root_dir, output_dir)

        else:
            relative_file_path = child_path.relative_to(root_dir)

            output_file = output_dir / relative_file_path

            if output_file.exists():
                print(f"output_file modified at {output_file.stat().st_mtime} input file modifies at {child_path.stat().st_mtime}")

            # if output_file.exists() and output_file.stat().st_mtime >= child_path.stat().st_mtime:
            #     continue
            
            output_parent = output_file.parent
            output_parent.mkdir(parents=True, exist_ok=True)

            if child_path.suffix == '.slang':
                full_output_file = output_file.with_suffix('.spv')
                compile_slang_shader(child_path, full_output_file, [root_dir, root_dir.parent, 'D:\\Source\\SahRenderer\\RenderCore\\extern'])
            
            elif child_path.suffix == '.glsl':
                # GLSL include file
                continue

            else:
                full_output_file = output_file.with_suffix(output_file.suffix + '.spv')
                compile_glsl_shader(child_path, full_output_file, [root_dir, root_dir.parent, 'D:\\Source\\SahRenderer\\RenderCore\\extern'])


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: compile_slang_shaders.py <input_directory> <output_directory>")
        exit()

    input_dir = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])

    print(f"Compilling shaders from {input_dir} to {output_dir}")

    vulkan_sdk_dir = Path(os.environ["VULKAN_SDK"])
    glslang_exe = vulkan_sdk_dir / "Bin" / "glslangValidator.exe"
    slang_exe = vulkan_sdk_dir / "Bin" / "slangc.exe"

    print(f"Using GLSL compiler {glslang_exe}")
    print(f"Using Slang compiler {slang_exe}")

    # .\slangc.exe hello-world.slang -profile glsl_460 -target spirv -entry main -o hello-world.spv

    # Iterate over the input directory. For every .slang file, compile it to the output directory. Create a folder structure mirroring the folder structure in the input directory

    compile_shaders_in_path(input_dir, input_dir, output_dir)

