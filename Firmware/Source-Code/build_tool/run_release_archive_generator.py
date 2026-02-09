import os
import subprocess
import zipfile
import run_build_tool as bt
import logging

def get_git_commit_hash():
    return bt.get_git_commit_hash()

def get_version_info():
    return bt.get_firmware_version()

def main():
    '''
    Main entry point for the release archive generator script.
    '''
    print("Running release archive generator script...")
    args = bt.parse_args()  # Reuse the argument parsing
    bt.generate_board_build_config(args.board)
    artifacts_collection = bt.run_build_tool(args)  # Run the build tool with the parsed arguments
    print(f"Artifacts collection: {artifacts_collection}")
    # Existing code to retrieve commit hash and version info
    git_commit_hash = bt.get_git_commit_hash()
    version_info = bt.get_firmware_version()

    # Assuming 'output' directory and other file collection logic is handled here
    build_tool_path = os.path.dirname(os.path.abspath(__file__))
    output_folder = "release"
    archive_folder_path = os.path.join(build_tool_path, "..", output_folder)
    files_to_zip = artifacts_collection
    artifacts_zip_name = f"cbgm-fih-ie-firmware-archive-{args.board}-v{version_info}_{git_commit_hash}.zip"
    create_zip_file(files_to_zip, archive_folder_path, artifacts_zip_name)

def create_zip_file(files, output_folder, archive_name):
    '''
    Create a ZIP file containing the specified files and directories.
    '''
    print(f"Creating ZIP file with the following files: {files}")
    # Ensure the output directory exists
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)  # Create the output folder if it does not exist

    # Full path for the zip file
    zip_file_path = os.path.join(output_folder, archive_name)

    # Open the zip file in write mode, which will overwrite the file if it already exists
    with zipfile.ZipFile(zip_file_path, 'w') as zipf:
        for file in files:
            if os.path.isfile(file):
                # Add the file under its basename, which is the name without any directory paths
                zipf.write(file, arcname=os.path.basename(file))
            elif os.path.isdir(file):
                # If it's a directory, add all files within it
                for root, _, file_names in os.walk(file):
                    for file_name in file_names:
                        file_path = os.path.join(root, file_name)
                        # Use relative path for files within the directory to maintain directory structure in the zip
                        zipf.write(file_path, arcname=os.path.relpath(file_path, start=file))
    print(f"Created or updated ZIP file at {zip_file_path}")

if __name__ == "__main__":
    main()
