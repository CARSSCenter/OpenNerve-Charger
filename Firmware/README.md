<img src="OpenNerve_logo.png" width="800" height="200">

# WPT Charger Embedded Software Development

## Generalities
This repository is dedicated to the development of the WPT Charger embedded software. 

## Getting Started
This section outlines the essential steps to establish a functional environment for building and flashing the application to the WPT Charger. If you are planning to engage in development, refer to the Development section for additional steps that must be followed.

### Install SEGGER IDE
The SEGGER IDE v8.10c is currently being used to compile and flash the application. You can download it [here](https://www.segger.com/downloads/embedded-studio). 

## Development
If you are involved in the development of this project, in addition to the Getting Started section, follow the next steps to set up your development environment.

### Install Python
Install [Python 3.11.3](https://www.python.org/downloads/).

ⓘ Note: If you are running Linux or macOS, consider using `pyenv` to install multiple, independent Python versions.

### Install Git
Install [Git](https://git-scm.com/downloads).

### Set build tool paths
Set build tool paths to use the build tool. Inside the `build_tool` path you will find the `config.ini.example` file. You should copy this, rename it to `config.ini`, and edit it to set your own paths. In addition, you should update the environment variables by adding the `bin` directory of SEGGER to the system's `PATH`. 
After this, you can run the build tool in a Windows PowerShell in build tool path using one of these commands depending on whether you want to build for the devkit or the PCBA:
```
py .\run_build_tool.py --board devkit
```
```
py .\run_build_tool.py --board pcba
```
