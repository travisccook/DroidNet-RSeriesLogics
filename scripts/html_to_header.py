#!/usr/bin/env python3
"""
Compress HTML files and generate C++ header files
"""

import os
import gzip
import re
from pathlib import Path

def minify_html(html):
    """Basic HTML minification that preserves JavaScript"""
    # Remove HTML comments (but not JS comments)
    html = re.sub(r'<!--.*?-->', '', html, flags=re.DOTALL)
    
    # Extract script contents to protect them
    scripts = []
    def protect_script(match):
        scripts.append(match.group(1))
        return f'<script>__SCRIPT_{len(scripts)-1}__</script>'
    
    # Temporarily replace script contents
    html = re.sub(r'<script[^>]*>(.*?)</script>', protect_script, html, flags=re.DOTALL)
    
    # Now minify the HTML parts
    # Remove extra whitespace between tags
    html = re.sub(r'>\s+<', '><', html)
    # Remove extra whitespace (but not in script placeholders)
    html = re.sub(r'\s+', ' ', html)
    # Remove whitespace around = in attributes
    html = re.sub(r'\s*=\s*', '=', html)
    
    # Restore script contents with minimal minification
    for i, script in enumerate(scripts):
        # Don't minify JavaScript - just restore it as-is
        # JavaScript minification can break function definitions
        html = html.replace(f'__SCRIPT_{i}__', script)
    
    return html.strip()

def compress_html_file(input_path, output_path, var_name):
    """Compress HTML file and generate C++ header"""
    
    # Read and minify HTML
    with open(input_path, 'r', encoding='utf-8') as f:
        html_content = f.read()
    
    minified = minify_html(html_content)
    print(f"  Original size: {len(html_content)} bytes")
    print(f"  Minified size: {len(minified)} bytes")
    
    # Compress with gzip
    compressed = gzip.compress(minified.encode('utf-8'), compresslevel=9)
    print(f"  Compressed size: {len(compressed)} bytes")
    print(f"  Compression ratio: {100 - (len(compressed) / len(html_content) * 100):.1f}%")
    
    # Generate C++ header
    with open(output_path, 'w') as f:
        f.write(f"// Auto-generated from {os.path.basename(input_path)}\n")
        f.write(f"// Original size: {len(html_content)} bytes\n")
        f.write(f"// Compressed size: {len(compressed)} bytes\n")
        f.write(f"#ifndef {var_name}_H\n")
        f.write(f"#define {var_name}_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        
        # Write compressed data
        f.write(f"const uint8_t {var_name}_GZ[] PROGMEM = {{\n")
        
        # Write bytes in rows of 16
        for i in range(0, len(compressed), 16):
            chunk = compressed[i:i+16]
            hex_bytes = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f"    {hex_bytes},\n")
        
        f.write("};\n\n")
        f.write(f"const size_t {var_name}_GZ_SIZE = {len(compressed)};\n\n")
        
        # Also store original size for Content-Length if needed
        f.write(f"const size_t {var_name}_ORIGINAL_SIZE = {len(minified)};\n\n")
        
        f.write(f"#endif // {var_name}_H\n")

def main():
    """Generate compressed headers for all modern UI files"""
    
    script_dir = Path(__file__).parent
    project_dir = script_dir.parent
    data_dir = project_dir / "data" / "modern"
    include_dir = project_dir / "build" / "generated"
    
    # Create include directory if it doesn't exist
    include_dir.mkdir(parents=True, exist_ok=True)
    
    print("HTML to Header Compression Tool")
    print("=" * 50)
    
    # Define files to process
    files_to_process = [
        ("index.html", "MODERN_INDEX"),
        ("wifi.html", "MODERN_WIFI"),
    ]
    
    for filename, var_name in files_to_process:
        input_path = data_dir / filename
        output_path = include_dir / f"{var_name.lower()}_html.h"
        
        if not input_path.exists():
            print(f"ERROR: {input_path} not found!")
            continue
        
        print(f"\nProcessing {filename}:")
        compress_html_file(input_path, output_path, var_name)
        print(f"  Generated: {output_path}")
    
    # Generate master include file
    master_header = include_dir / "modern_ui_html.h"
    with open(master_header, 'w') as f:
        f.write("// Auto-generated master include for Modern UI HTML\n")
        f.write("#ifndef MODERN_UI_HTML_H\n")
        f.write("#define MODERN_UI_HTML_H\n\n")
        
        for _, var_name in files_to_process:
            f.write(f'#include "{var_name.lower()}_html.h"\n')
        
        f.write("\n#endif // MODERN_UI_HTML_H\n")
    
    print(f"\nGenerated master header: {master_header}")
    print("\nDone! Headers generated in build/generated/")
    print("Note: Update WebPages.h include path to \"build/generated/modern_ui_html.h\"")

if __name__ == "__main__":
    main()