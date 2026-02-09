import argparse
import configparser
import subprocess
import glob
import os
import shutil
import logging
import re
import time

# Setup basic logging configuration
logger = logging.getLogger(__name__)

class CustomFormatter(logging.Formatter):

    info_debug_color = "\x1b[38;5;14m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_red = "\x1b[31;1m"
    reset = "\x1b[0m"
    format = "%(asctime)s - %(name)s - %(levelname)s - %(message)s (%(filename)s:%(lineno)d)"

    FORMATS = {
        logging.DEBUG: info_debug_color + format + reset,
        logging.INFO: info_debug_color + format + reset,
        logging.WARNING: yellow + format + reset,
        logging.ERROR: red + format + reset,
        logging.CRITICAL: bold_red + format + reset
    }

    def format(self, record):
        log_fmt = self.FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)
logger.setLevel(logging.DEBUG)
# create console handler with a higher log level
ch = logging.StreamHandler()
ch.setLevel(logging.DEBUG)
ch.setFormatter(CustomFormatter())
logger.addHandler(ch)

def parse_args():
    '''
    Parse command line arguments.
    '''
    parser = argparse.ArgumentParser(description='Build project with SEGGER Embedded Studio.')
    parser.add_argument('--config', type=str, choices=['production', 'manufacturing', 'development', 'Debug'], default='Debug', help='Build configuration.')
    parser.add_argument('--board', type=str, required=True, choices=['pcba','devkit'], help='Specify the board type (PCBA or Devkit).')
    return parser.parse_args()

def get_git_commit_hash():
    '''
    Get the first 8 characters of the current Git commit hash.
    '''
    try:
        # Capture the first 8 characters of the git commit hash
        commit_hash = subprocess.check_output(['git', 'rev-parse', 'HEAD']).decode('utf-8').strip()[:8]
        logger.info(f"Current Git commit hash (first 8 characters): {commit_hash}")
        return commit_hash
    except subprocess.CalledProcessError as e:
        logger.error("Failed to get Git commit hash.")
        return None

def get_firmware_version():
    '''
    Get the firmware version from the version.h file.
    '''
    version_file_path = os.path.join("..", "src/project", "version.h")
    try:
        with open(version_file_path, 'r') as file:
            for line in file:
                if "#define VER_MAJOR" in line:
                    # Extract the firmware version using a regular expression
                    major_version = re.search(r'\d', line)
                elif "#define VER_MINOR" in line:
                    minor_version = re.search(r'\d', line)
                elif "#define VER_REVISION" in line:
                    revision_version = re.search(r'\d', line)
            return f'{major_version.group(0)}.{minor_version.group(0)}.{revision_version.group(0)}'
    except Exception as e:
        logger.error(f"Failed to read firmware version: {e}")
    return None

def collect_artifact_files(artifacts_dir):
    '''
    Collect the artifact files from the specified directory.
    '''
    # List of file patterns to collect, adjust patterns according to your artifact types
    logger.info(f"Collecting artifact files from {artifacts_dir}.")
    patterns = ["*.hex", "*.bin", "*.map"]
    files = []
    for pattern in patterns:
        files.extend(glob.glob(os.path.join(artifacts_dir, pattern)))
    return files

def run_build_tool(args):
    '''
    Run the build tool to build the project and generate binaries files
    '''

    # Map the command line configuration to the actual build configuration names
    config_mapping = {
        'production': 'production',
        'manufacturing': 'manufacturing',
        'development': 'development',
        'Debug': 'Debug'
    }

    board_mapping = {
        'devkit': 'DEV_KIT',
        'pcba': 'PCBA'
    }

    # Load the SEGGER Embedded Studio from the config.ini file
    config = configparser.ConfigParser()
    config.read('config.ini')
    segger_path = config['segger']['SEGGER_PATH']

    # The path to the embuild bin directory
    embuild_bin_path = f'"{segger_path}\\bin"'

    # Project's details
    segger_ide_workspace_path = config['workspace']['SEGGER_IDE_WORKSPACE_PATH']
    project_name = 'hornet-wpt-charger'
    build_configuration = config_mapping[args.config]
    board_configuration = board_mapping[args.board]

    # Building the command for adding emBuild to PATH and building the project
    path_command = f'$env:path += \';{embuild_bin_path}\''
    # Get the current directory
    current_directory = os.getcwd()

    # Get the parent directory
    parent_directory = os.path.dirname(current_directory)
    
    clean_command = f'emBuild.exe -config "{build_configuration}" -clean -verbose -time -project "{project_name}" "{segger_ide_workspace_path}//{project_name}.emProject"'
    build_command = f'emBuild.exe -config "{build_configuration}" -rebuild -verbose -time -project "{project_name}" "{segger_ide_workspace_path}//{project_name}.emProject"'

    commit_hash = get_git_commit_hash()
    fw_version = get_firmware_version()
    logger.info(f'Building {project_name} firmware version {fw_version}_{commit_hash} in {args.config} build configuration')

    #Set the current path to the script's directory
    build_tool_path = os.path.dirname(os.path.abspath(__file__))
    current_path = build_tool_path
    build_configuration_artifacts_folder_path = os.path.join(current_path, "..\\src\\project\\project\\Output\\", args.config + '\\Exe\\')
 
    if not os.path.exists(build_configuration_artifacts_folder_path):
        # Create the folder
        os.makedirs(build_configuration_artifacts_folder_path)
        logger.info(f"Build Configuration Artifacts folder '{args.config}' created at {build_configuration_artifacts_folder_path}.")

    # Attempting to remove build configuration deprecated artifacts
    os.chdir(build_configuration_artifacts_folder_path)
    logger.info(f'Attempting to remove deprecated {args.config} build configuration artifacts {current_path}.')
    try:
        for file_extension in ['*.hex','*.elf','*.bin', '*.map']:
            deprecated_artifacts = glob.glob(f'{file_extension}')
            for deprecated_artifact in deprecated_artifacts:
                logger.info(f'Removing deprecated artifact {deprecated_artifact}')
                os.remove(deprecated_artifact)
        if file_extension != None:
            logger.info(f'Build configuration artifacts removed')
        else:
            logger.info(f'{args.config} build configuration folder is empty from artifacts.')
    except Exception as e:
        logger.info(f'Error trying to remove depracted artifacts:\n {e}')

    logger.info(f'Building {project_name} firmware version {fw_version}_{commit_hash} in {args.config} build configuration')

    #Execute the Clean command in PowerShell
    attempts = 0
    while (attempts < 2):
        try:
            subprocess.run(['powershell', '-Command', f'{clean_command}'], check=True) 
            break
        except Exception as e:
            attempts+=1
            logger.error(f'Error during SEGGER Clean Process (attempt #{attempts}): {e}')

    #Execute the Build command in PowerShell
    attempts = 0
    while (attempts < 2):
        try:
            subprocess.run(['powershell', '-Command', f'{path_command}; cd {segger_path}; {build_command}'], check=True) 
            break
        except Exception as e:
            attempts+=1
            logger.error(f'Error during SEGGER Build Process (attempt #{attempts}): {e}')
            # Check if the .metadata folder exists and remove it
            metadata_path = os.path.join(segger_ide_workspace_path.replace("\"",""), '.metadata')
            logger.info(f"Attempting to remove .metadata folder at {metadata_path} to ensure a clean project workspace")
            if os.path.exists(metadata_path):
                logger.info(f".metadata folder removed")
                shutil.rmtree(metadata_path)
            else:
               logger.info(f"Folder doesn't exists")

    logger.info('Build process completed.')

    # Copy s140_nrf52_7.2.0_softdevice.hex to the corresponding build configuration directory (..\{args.config}\)
    #Set the current path to the script's directory
    logger.info(f"Grabbing Softdevice from parent directory {parent_directory}")
    softdevice_tool_path = parent_directory
    softdevice_tool_path += "\\src\\core_layer\\sdk\\nrf5\\nRF5_SDK_17.1.0_ddde560\\components\\softdevice\\s140\\hex\\"
    os.chdir(softdevice_tool_path)
    logger.info(f"Copy s140_nrf52_7.2.0_softdevice.hex to the corresponding build configuration directory: {build_configuration_artifacts_folder_path}")
    shutil.copy(os.path.join(softdevice_tool_path, "s140_nrf52_7.2.0_softdevice.hex"), build_configuration_artifacts_folder_path)

    # Change to the parent directory and then enter the build configuration directory
    os.chdir(build_configuration_artifacts_folder_path)

    if commit_hash and fw_version:
        # Process .bin files
        bin_files = glob.glob('*.bin')
        for bin_file in bin_files:
            base_name = os.path.splitext(bin_file)[0]
            if base_name == 's140_nrf52_7.2.0_softdevice':
                continue
            new_name = f"{base_name}{args.board}_v{fw_version}_{commit_hash}.bin"
            try:
                os.rename(bin_file, new_name)
                logger.info(f"Renamed {bin_file} to {new_name}")
            except Exception as e:
                logger.error(f"Could not rename {bin_file} to {new_name}: {e}")

        # Process .elf files
        elf_files = glob.glob('*.elf')
        for elf_file in elf_files:
            base_name = os.path.splitext(elf_file)[0]
            if base_name == 's140_nrf52_7.2.0_softdevice':
                continue
            new_name = f"{base_name}{args.board}_v{fw_version}_{commit_hash}.elf"
            try:
                os.rename(elf_file, new_name)
                logger.info(f"Renamed {elf_file} to {new_name}")
            except Exception as e:
                logger.error(f"Could not rename {elf_file} to {new_name}: {e}")

        # Process .hex files
        hex_files = glob.glob('*.hex')
        for hex_file in hex_files:
            base_name = os.path.splitext(hex_file)[0]
            if base_name == 's140_nrf52_7.2.0_softdevice':
                continue
            new_name = f"{base_name}{args.board}_v{fw_version}_{commit_hash}.hex"
            try:
                os.rename(hex_file, new_name)
                logger.info(f"Renamed {hex_file} to {new_name}")
            except Exception as e:
                logger.error(f"Could not rename {hex_file} to {new_name}: {e}")

        # Process .map files
        map_files = glob.glob('*.map')
        for map_file in map_files:
            base_name = os.path.splitext(map_file)[0]
            if base_name == 's140_nrf52_7.2.0_softdevice':
                continue
            new_name = f"{base_name}{args.board}_v{fw_version}_{commit_hash}.map"
            try:
                os.rename(map_file, new_name)
                logger.info(f"Renamed {map_file} to {new_name}")
            except Exception as e:
                logger.error(f"Could not rename {map_file} to {new_name}: {e}")
    else:
        logger.error("Script cannot proceed without a Git commit hash and firmware version.")

    logger.info('Build tool execution completed.')
    logger.info(f'Build artifacts collected from {build_configuration_artifacts_folder_path}.')

    build_and_board_configuration_artifacts_folder_path = os.path.join(current_path, "..", f'{args.config}_{args.board}')
    logger.info('Attempting to move build configuration artifacts to build and board configuration artifacts folder.')
    logger.info(f'Build and board configuration artifacts folder path: {build_and_board_configuration_artifacts_folder_path}')
    if  os.path.exists(build_and_board_configuration_artifacts_folder_path):
        # Attempting to remove build configuration and board configuration deprecated artifacts
        shutil.rmtree(build_and_board_configuration_artifacts_folder_path)
    os.chdir(build_tool_path)
    logger.info(f'Moving build configuration artifacts to build and board configuration artifacts folder.')
    os.rename(build_configuration_artifacts_folder_path, build_and_board_configuration_artifacts_folder_path)
    logger.info(f'Build configuration artifacts moved to build and board configuration artifacts folder.')
    logger.info('Build tool execution completed.')
    return collect_artifact_files(build_and_board_configuration_artifacts_folder_path)

def get_board_definition(board):
    '''
    Get the board definition based on the board type.
    '''
    if board == 'devkit':
        return 'DEV_KIT'
    elif board == 'pcba':
        return 'PCBA'
    return None

def generate_board_build_config(board):
    '''
    Update the config.h file with the board to be used.
    '''
    logger.info(f'Generating board build configuration for {board} board.')
    logger.info(f'Updating the config.h file with the board definition.')
    config_file_path = '../src/project/config.h'
    board_definition = f'#define BOARD {get_board_definition(board)}\n'

    # Read the current content of the config file
    with open(config_file_path, 'r') as file:
        content = file.readlines()

    # Find and replace the board configuration lines
    with open(config_file_path, 'w') as file:
        board_line_found = False
        for line in content:
            if line.startswith('#define BOARD'):
                file.write(board_definition)
                board_line_found = True
            else:
                file.write(line)

        # If no board line was found, append the new line at the end
        if not board_line_found:
            file.write(board_definition)
    logger.info('Board build configuration updated.')

if __name__ == "__main__":
    args = parse_args()
    generate_board_build_config(args.board)
    run_build_tool(args)