#!/usr/bin/env python3
"""
Script to build Python package using manylinux1 Docker container.
Copies local directory, runs python -m build, and copies back the dist folder.
"""

import subprocess
import shutil
import os
import sys
from pathlib import Path
import tempfile
import argparse

# Configuration
CONTAINER_IMAGE = "quay.io/pypa/manylinux2014_x86_64"  # Faster builds, good compatibility
PYTHON_VERSION = "cp39-cp39"  # Change this to your desired Python version

# ANSI color codes
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color

def print_colored(message, color=Colors.NC):
    """Print colored message to stdout."""
    print(f"{color}{message}{Colors.NC}")

def run_command(cmd, check=True, capture_output=False):
    """Run a shell command and optionally capture output."""
    print_colored(f"Running: {' '.join(cmd)}", Colors.BLUE)
    try:
        result = subprocess.run(
            cmd, 
            check=check, 
            capture_output=capture_output, 
            text=True
        )
        return result
    except subprocess.CalledProcessError as e:
        print_colored(f"Command failed with exit code {e.returncode}", Colors.RED)
        if capture_output and e.stdout:
            print_colored(f"stdout: {e.stdout}", Colors.RED)
        if capture_output and e.stderr:
            print_colored(f"stderr: {e.stderr}", Colors.RED)
        raise

def check_docker():
    """Check if Docker is available and running."""
    try:
        # Check if docker command exists
        result = run_command(["docker", "--version"], capture_output=True)
        print_colored(f"Docker version: {result.stdout.strip()}", Colors.GREEN)
        
        # Check if Docker daemon is running
        run_command(["docker", "info"], capture_output=True)
        print_colored("Docker daemon is running", Colors.GREEN)
        
    except FileNotFoundError:
        print_colored("Error: Docker is not installed", Colors.RED)
        print_colored("Please install Docker from: https://docs.docker.com/get-docker/", Colors.YELLOW)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print_colored("Error: Docker is installed but not running", Colors.RED)
        if "permission denied" in e.stderr.lower():
            print_colored("Docker permission issue detected. Try one of:", Colors.YELLOW)
            print_colored("1. Add your user to docker group: sudo usermod -aG docker $USER", Colors.YELLOW)
            print_colored("2. Run with sudo: sudo python build_manylinux.py", Colors.YELLOW)
            print_colored("3. Start Docker daemon if not running", Colors.YELLOW)
        else:
            print_colored("Please start Docker daemon:", Colors.YELLOW)
            print_colored("- On Linux: sudo systemctl start docker", Colors.YELLOW)
            print_colored("- On macOS/Windows: Start Docker Desktop", Colors.YELLOW)
        sys.exit(1)

def clean_dist_directory():
    """Remove existing dist directory if it exists."""
    dist_path = Path("dist")
    if dist_path.exists():
        print_colored("Removing existing dist directory...", Colors.YELLOW)
        shutil.rmtree(dist_path)

def get_ignore_patterns():
    """Get list of patterns to ignore when copying files."""
    return [
        '.git',
        '__pycache__',
        '*.pyc',
        '.pytest_cache',
        'build',
        'dist',
        '*.egg-info',
        '.venv',
        'venv',
        '.env',
        '.DS_Store',
        '*.swp',
        '*.swo',
        '.idea',
        '.vscode'
    ]

def should_ignore(path, ignore_patterns):
    """Check if a path should be ignored based on patterns.""" 
    path_name = os.path.basename(path)
    for pattern in ignore_patterns:
        if pattern.startswith('*'):
            if path_name.endswith(pattern[1:]):
                return True
        elif path_name == pattern:
            return True
    return False

def copy_source_files(src_dir, dest_dir):
    """Copy source files to destination, excluding unnecessary files."""
    print_colored("Copying source files...", Colors.YELLOW)
    
    ignore_patterns = get_ignore_patterns()
    
    for root, dirs, files in os.walk(src_dir):
        # Filter out ignored directories
        dirs[:] = [d for d in dirs if not should_ignore(os.path.join(root, d), ignore_patterns)]
        
        # Create corresponding directory structure
        rel_root = os.path.relpath(root, src_dir)
        if rel_root == '.':
            dest_root = dest_dir
        else:
            dest_root = os.path.join(dest_dir, rel_root)
            os.makedirs(dest_root, exist_ok=True)
        
        # Copy files
        for file in files:
            src_file = os.path.join(root, file)
            if not should_ignore(src_file, ignore_patterns):
                dest_file = os.path.join(dest_root, file)
                shutil.copy2(src_file, dest_file)
    
    # Set proper permissions after copying
    for root, dirs, files in os.walk(dest_dir):
        os.chmod(root, 0o755)
        for file in files:
            os.chmod(os.path.join(root, file), 0o644)

def build_in_container(build_dir, python_version):
    """Build the package inside the manylinux container."""
    print_colored("Building package in manylinux container...", Colors.YELLOW)
    
    # Get current user ID and group ID to avoid permission issues
    uid = os.getuid()
    gid = os.getgid()
    
    # Docker command to run the build
    docker_cmd = [
        "docker", "run", "--rm",
        "--cpus", str(os.cpu_count()),  # Use all available CPU cores
        "-v", f"{build_dir}:/workspace",
        "-w", "/workspace",
        "-e", "HOME=/workspace",
        "-e", f"USER=builder",
        "--user", f"{uid}:{gid}",
        CONTAINER_IMAGE,
        "/bin/bash", "-c",
        f"""
        set -e
        echo "Setting up user environment..."
        mkdir -p /workspace/.local
        export PATH="/workspace/.local/bin:$PATH"
        export PYTHONUSERBASE="/workspace/.local"
        
        echo "Installing system dependencies..."
        yum install -y ninja-build || echo "ninja-build not available, will install via pip"
        
        echo "Installing build dependencies..."
        /opt/python/{python_version}/bin/pip install --user --upgrade pip build auditwheel
        
        echo "Building package..."
        /opt/python/{python_version}/bin/python -m build --verbose
        
        echo "Build completed successfully!"
        ls -la dist/
        
        echo "Repairing wheels with auditwheel..."
        mkdir -p /workspace/wheelhouse
        for wheel in dist/*.whl; do
            echo "Repairing $wheel..."
            /workspace/.local/bin/auditwheel repair "$wheel" --wheel-dir /workspace/wheelhouse
        done
        
        echo "Manylinux wheels created:"
        ls -la /workspace/wheelhouse/
        
        echo "Cleaning up user-specific files..."
        rm -rf /workspace/.local
        """
    ]
    
    run_command(docker_cmd)

def copy_dist_back(build_dir, current_dir):
    """Copy the dist directory back to the current directory."""
    src_dist = os.path.join(build_dir, "dist")
    src_wheelhouse = os.path.join(build_dir, "wheelhouse")
    dest_dist = os.path.join(current_dir, "dist")
    
    # Copy original dist first
    if os.path.exists(src_dist):
        print_colored("Copying original dist directory back...", Colors.YELLOW)
        shutil.copytree(src_dist, dest_dist)
    
    # Copy manylinux wheels from wheelhouse
    if os.path.exists(src_wheelhouse):
        print_colored("Copying manylinux wheels from wheelhouse...", Colors.YELLOW)
        for file in os.listdir(src_wheelhouse):
            if file.endswith('.whl'):
                src_file = os.path.join(src_wheelhouse, file)
                dest_file = os.path.join(dest_dist, file)
                shutil.copy2(src_file, dest_file)
        
        print_colored("Manylinux wheels copied to ./dist/", Colors.GREEN)
        
        # List the contents
        print_colored("Built packages:", Colors.GREEN)
        for file in os.listdir(dest_dist):
            if file.endswith('.whl'):
                wheel_type = "manylinux" if "manylinux" in file else "linux"
                print(f"  {file} ({wheel_type})")
            else:
                print(f"  {file}")
    else:
        print_colored("Warning: No wheelhouse directory found after build", Colors.YELLOW)

def main():
    parser = argparse.ArgumentParser(description="Build Python package using manylinux1 container")
    parser.add_argument("--python-version", default=PYTHON_VERSION, 
                       help=f"Python version to use (default: {PYTHON_VERSION})")
    parser.add_argument("--keep-temp", action="store_true",
                       help="Keep temporary build directory for debugging")
    
    args = parser.parse_args()
    
    print_colored("Starting manylinux build process...", Colors.GREEN)
    
    # Check prerequisites
    check_docker()
    
    # Clean up existing dist
    clean_dist_directory()
    
    # Create temporary build directory
    current_dir = os.getcwd()
    
    with tempfile.TemporaryDirectory() as temp_dir:
        build_dir = os.path.join(temp_dir, "build")
        os.makedirs(build_dir)
        
        print_colored(f"Using temporary build directory: {build_dir}", Colors.BLUE)
        
        try:
            # Copy source files
            copy_source_files(current_dir, build_dir)
            
            # Build in container
            build_in_container(build_dir, args.python_version)
            
            # Copy results back
            copy_dist_back(build_dir, current_dir)
            
            print_colored("Build process completed successfully!", Colors.GREEN)
            
        except Exception as e:
            print_colored(f"Build process failed: {str(e)}", Colors.RED)
            
            # Try to fix permissions before cleanup
            try:
                print_colored("Attempting to fix permissions for cleanup...", Colors.YELLOW)
                subprocess.run([
                    "docker", "run", "--rm",
                    "-v", f"{build_dir}:/workspace",
                    "--user", "root",
                    CONTAINER_IMAGE,
                    "/bin/bash", "-c",
                    "find /workspace -type d -exec chmod 755 {} \\; && find /workspace -type f -exec chmod 644 {} \\;"
                ], check=False)
            except:
                pass  # Ignore errors during permission fixing
            
            if args.keep_temp:
                print_colored(f"Temporary directory preserved at: {build_dir}", Colors.YELLOW)
            sys.exit(1)

if __name__ == "__main__":
    main()