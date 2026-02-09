import os
import shutil
import subprocess
import configparser
import argparse
import logging
import glob

# Setup basic logging configuration
logger = logging.getLogger(__name__)
class CustomFormatter(logging.Formatter):

    info_debug_color = "\x1b[38;5;165m"
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

# Parse command line arguments
parser = argparse.ArgumentParser(description='Flash Artifacts with JLINK tool.')
parser.add_argument('--config', type=str, choices=['production', 'manufacturing', 'development', 'Debug'], default='Debug', help='Build configuration.')
parser.add_argument('--board', type=str,  required=True, choices=['pcba','devkit'], help='Specify the board type (PCBA or Devkit).')
args = parser.parse_args()

# Load the JLINK_EXE_PATH from the config.ini file
config = configparser.ConfigParser()
config.read('config.ini')

# Set the jlink_path variable with the full path to JLink.exe
jlink_path = config['jlink']['JLINK_EXE_PATH']

# Define your JLink commands

if __name__ == "__main__":
    logger.info(f'Starting Flash Tool execution for {args.config} build configuration and {args.board} board configuration.')
    # Set the current path to the script's directory
    current_path = os.path.dirname(os.path.abspath(__file__))

    # Print the path of the JLink_connect_Focus.jlink file
    jlink_file_path = os.path.join(current_path, "jlink_connect_hornet_wpt_charger_firmware_profile.jlink")
    logger.info(f"The path of the jlink_connect_hornet_wpt_charger_firmware_profile.jlink file is: {jlink_file_path}")

    # Change to the parent directory and then enter the build configuration directory
    build_configuration_artifacts_folder_path = os.path.join(current_path, "..", f'{args.config}_{args.board}')
    logger.info(f"Grabbing artifacts from {build_configuration_artifacts_folder_path}")
    os.chdir(build_configuration_artifacts_folder_path)
    softdevice = glob.glob(f's140_nrf52_7.2.0_softdevice.hex')
    if softdevice == []:
        logger.error(f"Could not find the softdevice artifact in '{build_configuration_artifacts_folder_path}'")
        logger.error("Make sure the softdevice artifact is present in the build configuration folder.")
        exit(1)
    softdevice_artifact_path = os.path.join(build_configuration_artifacts_folder_path, softdevice[0])
    wpt_fw_image = glob.glob(f'hornet-wpt-charger_{args.config}_{args.board}*.bin')
    if wpt_fw_image == []:
        logger.error(f"Could not find the WPT firmware image artifact matching '{args.config}' build configuration and '{args.board}' board configuration  in '{build_configuration_artifacts_folder_path}'")
        logger.error("Make sure the WPT firmware image artifact is present in the build configuration folder.")
        exit(1)
    wpt_fw_image_artifact_path = os.path.join(build_configuration_artifacts_folder_path, wpt_fw_image[0])

    jlink_commands = f"""
    device nRF52840_xxAA
    USB
    si SWD
    speed 4000
    jtagconf -1,-1
    connect
    st
    h
    erase 0 0 noreset
    loadfile {softdevice_artifact_path} 0x00000000
    verifybin {softdevice_artifact_path} 0x00000000
    Sleep 100
    loadfile {wpt_fw_image_artifact_path} 0x00027000
    verifybin {wpt_fw_image_artifact_path} 0x00027000
    r
    exit
    """
    # Save the commands to a temporary script file
    with open(jlink_file_path, 'w') as file:
        file.write(jlink_commands)

    # Execute the command with JLink.exe
    process = subprocess.run(['powershell', '-Command', f'cd {jlink_path}; jlink.exe -CommandFile {jlink_file_path}'], check=True)
    # Verify the value of ERRORLEVEL
    if process.returncode == 0:
        logger.info("The script execution was successful. Firmware was flashed")
    else:
        logger.error("There was an error during the script execution.", process.stderr)

    # Change back to the original directory
    os.chdir(current_path)