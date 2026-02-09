import subprocess
import logging
import os
import configparser

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


# Load the CPP_CHECK_EXECUTABLE_PATH from the config.ini file
config = configparser.ConfigParser()
config.read('config.ini')
cpp_check_executable_path = config['cppcheck']['CPP_CHECK_EXECUTABLE_PATH'].strip("\"")

def run_cppcheck(source_path, includes_path):
    logger.info("Running cppcheck")
    logger.info(f"cppcheck executable path: {cpp_check_executable_path}")
    # Define the command to run cppcheck
    command = [f"{cpp_check_executable_path}cppcheck.exe","--enable=all",  f"-I{includes_path}"," --quiet","--inconclusive", "--suppress=missingIncludeSystem", source_path]

    # Run the command using subprocess
    try:
        # Execute the command
        logger.info(f"Running command: {' '.join(command)}")
        subprocess.run(command, check=True)
    except subprocess.CalledProcessError as e:
        # If cppcheck returns a non-zero exit code, print the error
        logger.error(f"Error: {e}")

if __name__ == "__main__":
    logger.info("Start static analysis check")
    # Path to the directory containing the files to check
    build_tool_path = os.path.dirname(os.path.abspath(__file__))
    logger.info(f"Build tool path: {build_tool_path}")
    source_files_path = os.path.abspath(os.path.join(build_tool_path, '..', 'src'))
    include_file_path = os.path.abspath(os.path.join(build_tool_path, '..', 'include'))
    logger.info(f"Running static analysis on {source_files_path}")
    # Call the function with the path
    run_cppcheck(source_files_path, include_file_path)

