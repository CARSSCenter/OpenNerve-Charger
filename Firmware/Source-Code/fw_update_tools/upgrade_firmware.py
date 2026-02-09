import argparse
import subprocess

def upgrade_firmware(zip_file, com_port):
    """
    Flash firmware using nrfutil dfu serial command
    
    Args:
        zip_file: Path to the firmware zip package
        com_port: COM port to use
    """
    cmd = [
        "nrfutil", "dfu", "serial",
        "-pkg", zip_file,
        "-p", com_port,
        "-b", "115200",
        "-fc", "0"
    ]
    
    print(f"Upgrading firmware {zip_file} to {com_port}...")
    subprocess.run(cmd, check=True)
    print("Firmware upgrade completed successfully.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upgrade firmware using nrfutil dfu serial")
    parser.add_argument("zip_file", help="Firmware package zip file")
    parser.add_argument("com_port", help="COM port")
    
    args = parser.parse_args()
    
    upgrade_firmware(args.zip_file, args.com_port)