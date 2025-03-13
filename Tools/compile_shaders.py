'''Compiles slang shaders for Vulkan 1.3 SPIR-V

Basic usage: compile_slang_shaders.py <input_directory> <output_directory>
'''


import os
import subprocess
import sys
from pathlib import Path


glsl_extensions = ['.vert', '.geom', '.frag', '.comp']


def are_dependencies_modified(depfile_path, last_compile_time):    
    with open(depfile_path, 'r') as depfile:
        for dependency in depfile:
            dependency_path = Path(dependency.strip())
            if not dependency_path.exists():
                print(f"Dependency {dependency_path} does not exist, recompiling shader")
                return True
            
            if dependency_path.stat().st_mtime >= last_compile_time:
                print(f"Dependency {dependency_path} has been modified, recompiling shader")
                return True

    print("All dependencies are up-to-date")
    return False


def compile_slang_shader(input_file, output_file, include_directories, defines=[], entry_point='main'):
    depfile_path = output_file.with_suffix('.deps')
    if output_file.exists():
        # If the input file is older than the output file, don't recompile
        if input_file.stat().st_mtime < output_file.stat().st_mtime:
            return

        # If the dependencies in the dependency file are older than the output file, don't recompile
        if depfile_path.exists() and not are_dependencies_modified(depfile_path, output_file.stat().st_mtime):
            return

    command = [str(slang_exe), str(input_file), '-profile', 'glsl_460', '-target', 'spirv', '-entry', entry_point, '-fvk-use-scalar-layout', '-g', '-O0', '-o', str(output_file)]
    for dir in include_directories:
        command.append('-I')
        command.append(str(dir))

    for define in defines:
        command.append('-D')
        command.append(define)

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


def compile_material(input_file, output_file, include_directories):
    '''Compiles a material with all its variants
    '''

    print(f"Compiling material {input_file}")

    base_stem = output_file.stem

    # Shadow

    shadow_stem = base_stem + '_shadow'
    shadow_filename = output_file.with_stem(shadow_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, shadow_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_CSM=1', 'SAH_MULTIVIEW=1'], 'main_vs')
    
    shadow_masked_stem = base_stem + '_shadow_masked'
    shadow_masked_vs_filename = output_file.with_stem(shadow_masked_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, shadow_masked_vs_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_MASKED=1', 'SAH_CSM=1', 'SAH_MULTIVIEW=1'], 'main_vs')
    shadow_masked_fs_filename = output_file.with_stem(shadow_masked_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, shadow_masked_fs_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_MASKED=1', 'SAH_CSM=1', 'SAH_MULTIVIEW=1'], 'main_fs')

    # RSM 

    rsm_stem = base_stem + '_rsm'
    rsm_vs_filename = output_file.with_stem(rsm_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, rsm_vs_filename, include_directories, ['SAH_RSM=1', 'SAH_MULTIVIEW=1'], 'main_vs')    
    rsm_fs_filename = output_file.with_stem(rsm_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, rsm_fs_filename, include_directories, ['SAH_RSM=1', 'SAH_MULTIVIEW=1'], 'main_fs')
    
    rsm_masked_stem = base_stem + '_rsm_masked'
    rsm_masked_vs_filename = output_file.with_stem(rsm_masked_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, rsm_masked_vs_filename, include_directories, ['SAH_MASKED=1', 'SAH_RSM=1', 'SAH_MULTIVIEW=1'], 'main_vs')    
    rsm_masked_fs_filename = output_file.with_stem(rsm_masked_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, rsm_masked_fs_filename, include_directories, ['SAH_MASKED=1', 'SAH_RSM=1', 'SAH_MULTIVIEW=1'], 'main_fs')

    # Depth prepass

    prepass_stem = base_stem + '_prepass'
    prepass_filename = output_file.with_stem(prepass_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, prepass_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_MAIN_VIEW=1'], 'main_vs')
    
    prepass_masked_stem = base_stem + '_prepass_masked'
    prepass_masked_vs_filename = output_file.with_stem(prepass_masked_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, prepass_masked_vs_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_MASKED=1', 'SAH_MAIN_VIEW=1'], 'main_vs')
    prepass_masked_fs_filename = output_file.with_stem(prepass_masked_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, prepass_masked_fs_filename, include_directories, ['SAH_DEPTH_ONLY=1', 'SAH_MASKED=1', 'SAH_MAIN_VIEW=1'], 'main_fs')

    # Gbuffer

    gbuffer_stem = base_stem + '_gbuffer'
    gbuffer_vs_filename = output_file.with_stem(gbuffer_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, gbuffer_vs_filename, include_directories, ['SAH_MAIN_VIEW=1'], 'main_vs')    
    gbuffer_fs_filename = output_file.with_stem(gbuffer_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, gbuffer_fs_filename, include_directories, ['SAH_MAIN_VIEW=1'], 'main_fs')
    
    gbuffer_masked_stem = base_stem + '_gbuffer_masked'
    gbuffer_masked_vs_filename = output_file.with_stem(gbuffer_masked_stem).with_suffix('.vert.spv')
    compile_slang_shader(input_file, gbuffer_masked_vs_filename, include_directories, ['SAH_MASKED=1', 'SAH_MAIN_VIEW=1'], 'main_vs')    
    gbuffer_masked_fs_filename = output_file.with_stem(gbuffer_masked_stem).with_suffix('.frag.spv')
    compile_slang_shader(input_file, gbuffer_masked_fs_filename, include_directories, ['SAH_MASKED=1', 'SAH_MAIN_VIEW=1'], 'main_fs')


def compile_glsl_shader(input_file, output_file, include_directories):
    if output_file.exists() and input_file.stat().st_mtime < output_file.stat().st_mtime:
        return
        
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
    include_paths = [root_dir, root_dir.parent, 'D:\\Source\\SahRenderer\\RenderCore\\extern']

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
            
            output_parent = output_file.parent
            output_parent.mkdir(parents=True, exist_ok=True)

            if child_path.suffix == '.slang':
                if child_path.match('materials/*'):
                    compile_material(child_path, output_file, include_paths)
                else:                    
                    compile_slang_shader(child_path, output_file, include_paths)

            elif child_path.suffix in glsl_extensions:
                compile_glsl_shader(child_path, output_file, include_paths)


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

