import argparse
import subprocess
import shutil

def generate_fota_image_zip(version):
   hex_file = f"hornet-wpt-charger_Debug_v{version}.hex"
   zip_file = f"v{version}_hornet-wpt-charger.zip"
   source_hex = "../src/project/project/Output/Debug/Exe/hornet-wpt-charger_Debug_.hex"

   shutil.copy2(source_hex, hex_file)

   cmd = [
       "./nrfutil", "pkg", "generate",
       "--hw-version", "52",
       "--application-version", str(version),
       "--application", hex_file,
       "--sd-req", "0x100",
       "--key-file", "priv.pem",
       zip_file
   ]
   
   subprocess.run(cmd, check=True)

if __name__ == "__main__":
   parser = argparse.ArgumentParser(description="Generate firmware package with version")
   parser.add_argument("version", type=int, help="Firmware version number")
   args = parser.parse_args()
   
   generate_fota_image_zip(args.version)