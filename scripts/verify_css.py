"""
CSS Syntax Verifier for SolveSpace GTK4 CSS files
This script validates CSS files to catch syntax errors before they cause
crashes or layout problems in the GTK4 interface.
"""

import os
import sys
import re
import subprocess
import glob
from pathlib import Path

def find_css_files(base_dir):
    """Find all CSS files in the project."""
    css_files = []
    
    css_files.extend(glob.glob(f"{base_dir}/src/platform/css/*.css"))
    
    css_header_files = glob.glob(f"{base_dir}/src/platform/css/*.css.h")
    
    cpp_files = glob.glob(f"{base_dir}/src/platform/*.cpp")
    
    return css_files, css_header_files, cpp_files

def extract_css_from_header(header_file):
    """Extract CSS content from a .css.h file."""
    with open(header_file, 'r') as f:
        content = f.read()
    
    css_match = re.search(r'R"css\((.*?)\)css"', content, re.DOTALL)
    if css_match:
        return css_match.group(1)
    return None

def extract_css_from_cpp(cpp_file):
    """Extract CSS content from C++ files with raw strings."""
    with open(cpp_file, 'r') as f:
        content = f.read()
    
    css_blocks = []
    
    css_matches = re.finditer(r'R"css\((.*?)\)css"', content, re.DOTALL)
    for match in css_matches:
        css_blocks.append(match.group(1))
    
    css_string_matches = re.finditer(r'const char\* (?:.*?)css(?:.*?) = \s*(?:"(.*?)";|R"(.*?)")', content, re.DOTALL)
    for match in css_string_matches:
        if match.group(1):  # Regular string
            css_content = match.group(1).replace('\\"', '"').replace('\\n', '\n')
            css_blocks.append(css_content)
        elif match.group(2):  # Raw string
            css_blocks.append(match.group(2))
    
    return css_blocks

def validate_css(css_content, filename):
    """Validate CSS syntax using a temporary file and external validator."""
    if not css_content:
        return True, "Empty CSS content"
    
    temp_file = f"/tmp/solvespace_css_verify_{os.path.basename(filename)}"
    with open(temp_file, 'w') as f:
        f.write(css_content)
    
    try:
        result = subprocess.run(
            ["stylelint", "--config", "stylelint.config.js", temp_file],
            capture_output=True,
            text=True
        )
        os.remove(temp_file)
        
        if result.returncode != 0:
            return False, result.stderr or result.stdout
        return True, "CSS syntax is valid"
    except FileNotFoundError:
        return simple_css_validation(css_content, filename)
    finally:
        if os.path.exists(temp_file):
            os.remove(temp_file)

def simple_css_validation(css_content, filename):
    """Simple CSS validation for when stylelint is not available."""
    errors = []
    
    if css_content.count('{') != css_content.count('}'):
        errors.append(f"Unbalanced braces: {css_content.count('{')} opening vs {css_content.count('}')} closing")
    
    lines = css_content.split('\n')
    for i, line in enumerate(lines):
        line = line.strip()
        if ':' in line and not line.endswith('{') and not line.endswith('}') and not line.endswith(';') and not line.endswith('*/'):
            errors.append(f"Line {i+1}: Missing semicolon: {line}")
    
    if css_content.count('/*') != css_content.count('*/'):
        errors.append(f"Unclosed comments: {css_content.count('/*')} opening vs {css_content.count('*/')} closing")
    
    if errors:
        return False, "\n".join(errors)
    return True, "CSS syntax is valid (basic checks only)"

def main():
    """Main function to validate all CSS files."""
    if len(sys.argv) > 1:
        base_dir = sys.argv[1]
    else:
        base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    css_files, css_header_files, cpp_files = find_css_files(base_dir)
    
    print(f"Found {len(css_files)} CSS files, {len(css_header_files)} CSS header files, and {len(cpp_files)} C++ files to check")
    
    all_valid = True
    
    for css_file in css_files:
        with open(css_file, 'r') as f:
            css_content = f.read()
        
        valid, message = validate_css(css_content, css_file)
        if not valid:
            all_valid = False
            print(f"Error in {css_file}:\n{message}\n")
        else:
            print(f"✓ {css_file}: {message}")
    
    for header_file in css_header_files:
        css_content = extract_css_from_header(header_file)
        if css_content:
            valid, message = validate_css(css_content, header_file)
            if not valid:
                all_valid = False
                print(f"Error in {header_file}:\n{message}\n")
            else:
                print(f"✓ {header_file}: {message}")
    
    for cpp_file in cpp_files:
        css_blocks = extract_css_from_cpp(cpp_file)
        for i, css_content in enumerate(css_blocks):
            valid, message = validate_css(css_content, f"{cpp_file}:block{i+1}")
            if not valid:
                all_valid = False
                print(f"Error in {cpp_file} (CSS block {i+1}):\n{message}\n")
            else:
                print(f"✓ {cpp_file} (CSS block {i+1}): {message}")
    
    if not all_valid:
        print("CSS validation failed!")
        sys.exit(1)
    else:
        print("All CSS files are valid!")
        sys.exit(0)

if __name__ == "__main__":
    main()
