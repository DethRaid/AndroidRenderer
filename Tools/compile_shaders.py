'''Compiles slang shaders for Vulkan 1.3 SPIR-V

Basic usage: compile_slang_shaders.py <input_directory> <output_directory>
'''


import os
import subprocess
import sys
from pathlib import Path


glsl_extensions = ['.vert', '.geom', '.frag', '.comp']


def compile_slang_shader(input_file, output_file, include_directories):
    command = [str(slang_exe), str(input_file), '-profile', 'glsl_460', '-target', 'spirv', '-entry', 'main', '-fvk-use-scalar-layout', '-g', '-O0', '-o', str(output_file)]
    for dir in include_directories:
        command.append('-I')
        command.append(str(dir))

    print(f"Compiling {input_file} as Slang")
    print(f"output_file={output_file}")
    subprocess.run(command)

    # use -output-includes to get a list of included files, de-duplicate that to get a dependency list, recompile the shader if any of its dependencies have changed
    includes_command = command + ['-output-includes']    
    output = subprocess.run(includes_command, capture_output=True)

    # Collect dependencies, write to a file
    output_lines = output.stderr.splitlines()
    dependencies = set()
    for line in output_lines:
        stringline = line.decode()
        if stringline.startswith('(0): note: include'):
            dependencies.add(stringline[19:].strip()[1:-1])
    
    dependencies_filename = output_file.with_suffix('.deps')
    with open(dependencies_filename, 'w') as f:
        for dependency in dependencies:
            f.write(f"{dependency}\n")


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


def are_dependencies_modified(depfile_path, last_compile_time):    
    with open(depfile_path, 'r') as depfile:
        for dependency in depfile:
            dependency_path = Path(dependency)
            if not dependency_path.exists():
                print(f"Dependency {dependency_path} does not exist, recompiling shader")
                return True
            
            if dependency_path.stat().st_mtime >= last_compile_time:
                print(f"Dependency {dependency_path} has been modified, recompiling shader")
                return True

    print("All dependencies are up-to-date")
    return False


def compile_shaders_in_path(path, root_dir, output_dir):
    for child_path in path.iterdir():
        if child_path.is_dir():
            compile_shaders_in_path(child_path, root_dir, output_dir)

        else:
            relative_file_path = child_path.relative_to(root_dir)

            output_file = output_dir / relative_file_path
            if child_path.suffix == '.slang':
                output_file = output_file.with_suffix('.spv')
            else:
                output_file = output_file.with_suffix(output_file.suffix + '.spv')

            depfile_path = output_file.with_suffix('.deps')
            needs_compile = True
            if output_file.exists():
                # If the input file is older than the output file, don't recompile
                if child_path.stat().st_mtime < output_file.stat().st_mtime:
                    needs_compile = False

                # If the dependencies in the dependency file are older than the output file, don't recompile
                if depfile_path.exists() and not are_dependencies_modified(depfile_path, output_file.stat().st_mtime):
                    needs_compile = False

            if not needs_compile:
                continue
            
            output_parent = output_file.parent
            output_parent.mkdir(parents=True, exist_ok=True)

            if child_path.suffix == '.slang':
                compile_slang_shader(child_path, output_file, [root_dir, root_dir.parent, 'D:\\Source\\SahRenderer\\RenderCore\\extern'])
            
            elif child_path.suffix == '.glsl':
                # GLSL include file
                continue

            elif child_path.suffix in glsl_extensions:
                compile_glsl_shader(child_path, output_file, [root_dir, root_dir.parent, 'D:\\Source\\SahRenderer\\RenderCore\\extern'])


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

