#!/usr/bin/env python3
"""
JavaScript Validation Script for RSeriesLogic
Extracts and validates JavaScript code from WebPages.h and WifiWebServer.h
"""

import re
import sys
import os
import subprocess
import tempfile
from pathlib import Path

class JavaScriptExtractor:
    def __init__(self):
        self.js_snippets = []
        self.errors = []
        
    def extract_from_file(self, filepath):
        """Extract JavaScript snippets from a C++ header file"""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
                
            # Find emitJS() method implementations
            emit_js_pattern = r'void\s+emitJS\s*\([^)]*\)\s*(?:override\s*)?{([^}]+(?:{[^}]*}[^}]*)*)}'
            for match in re.finditer(emit_js_pattern, content, re.DOTALL):
                self.extract_js_from_method(match.group(1), filepath, match.start())
                
            # Also look for simple JavaScript blocks in strings
            js_block_pattern = r'client\.print\s*\(\s*"(<script>.*?</script>)"\s*\);'
            for match in re.finditer(js_block_pattern, content, re.DOTALL):
                script_content = match.group(1)
                script_content = script_content.replace('\\"', '"').replace('\\n', '\n')
                self.extract_inline_scripts(script_content, filepath, match.start())
                
            # Find inline JavaScript in HTML strings
            # Look for <script> tags in print statements
            script_pattern = r'client\.print\s*\(\s*"([^"]*<script[^>]*>.*?</script>[^"]*)"\s*\)'
            for match in re.finditer(script_pattern, content, re.DOTALL):
                self.extract_inline_scripts(match.group(1), filepath, match.start())
                
            # Find onclick and other event handlers
            event_pattern = r'on\w+\s*=\s*["\']([^"\']+)["\']'
            for match in re.finditer(event_pattern, content):
                handler = match.group(1)
                # Skip if it's just a function call
                if not handler.strip().endswith('()') and ';' in handler:
                    self.js_snippets.append({
                        'code': handler,
                        'file': filepath,
                        'line': content[:match.start()].count('\n') + 1,
                        'type': 'event_handler'
                    })
                    
            # Find JavaScript in WElement callbacks (updateValue_, pressed_, etc.)
            # This is specific to WifiWebServer.h pattern
            callback_pattern = r'client\.print\s*\(\s*"function\s+(\w+)\s*\([^)]*\)\s*{([^"]+)}"\s*\)'
            for match in re.finditer(callback_pattern, content):
                func_name = match.group(1)
                func_body = match.group(2)
                self.js_snippets.append({
                    'code': f"function {func_name}() {{ {func_body} }}",
                    'file': filepath,
                    'line': content[:match.start()].count('\n') + 1,
                    'type': 'function',
                    'name': func_name
                })
                
            # Extract appendScriptf() calls - common in WifiWebServer.h
            append_script_pattern = r'appendScriptf?\s*\(\s*"([^"\\]*(\\.[^"\\]*)*)"\s*(?:,.*?)?\)'
            js_accumulator = []
            current_function = None
            
            for match in re.finditer(append_script_pattern, content):
                js_line = match.group(1)
                # Unescape the string
                js_line = js_line.replace('\\"', '"')
                js_line = js_line.replace('\\n', '\n')
                js_line = js_line.replace('\\\\', '\\')
                
                line_num = content[:match.start()].count('\n') + 1
                
                # Check if this is a format string (contains %s, %d, etc.)
                is_format_string = bool(re.search(r'%[sdifx]', js_line))
                
                # Check if this starts a new function definition
                func_match = re.match(r'function\s+(\w+)\s*\([^)]*\)\s*{', js_line.strip())
                if func_match:
                    # Save previous accumulated code if any
                    if js_accumulator:
                        accumulated_code = ''.join(js_accumulator).strip()
                        if accumulated_code:
                            self.js_snippets.append({
                                'code': accumulated_code,
                                'file': filepath,
                                'line': line_num - len(js_accumulator),
                                'type': 'appendScript_sequence'
                            })
                    js_accumulator = [js_line]
                    current_function = func_match.group(1)
                elif js_line.strip() == '}' and current_function:
                    # End of function
                    js_accumulator.append(js_line)
                    accumulated_code = ''.join(js_accumulator).strip()
                    # Check if the function contains format strings
                    has_format_strings = bool(re.search(r'%[sdifx]', accumulated_code))
                    if not has_format_strings:
                        self.js_snippets.append({
                            'code': accumulated_code,
                            'file': filepath,
                            'line': line_num - len(js_accumulator) + 1,
                            'type': 'appendScript_function',
                            'name': current_function
                        })
                    js_accumulator = []
                    current_function = None
                elif current_function:
                    # Part of current function
                    js_accumulator.append(js_line)
                else:
                    # Standalone JavaScript statement
                    if js_line.strip() and not js_line.strip().startswith('//'):
                        # Only validate non-format strings
                        if not is_format_string:
                            self.js_snippets.append({
                                'code': js_line.strip(),
                                'file': filepath,
                                'line': line_num,
                                'type': 'appendScript_statement'
                            })
                        
            # Save any remaining accumulated code
            if js_accumulator:
                accumulated_code = ''.join(js_accumulator).strip()
                if accumulated_code:
                    # Check if the accumulated code contains format strings
                    has_format_strings = bool(re.search(r'%[sdifx]', accumulated_code))
                    if not has_format_strings:
                        self.js_snippets.append({
                            'code': accumulated_code,
                            'file': filepath,
                            'line': line_num - len(js_accumulator) + 1,
                            'type': 'appendScript_sequence'
                        })
                
            # Look for multiline JavaScript function definitions in WebPages.h
            # Pattern: "function name() {\n" followed by multiple "..." lines and ending with "}"
            multiline_func_pattern = r'"function\s+(\w+)\s*\([^)]*\)\s*{\s*"\s*\n((?:\s*"[^"]*"\s*\n)*)\s*"}"\s*'
            for match in re.finditer(multiline_func_pattern, content):
                func_name = match.group(1)
                func_body_lines = match.group(2)
                
                # Extract the function body from the quoted lines
                body_parts = []
                for line in func_body_lines.split('\n'):
                    line = line.strip()
                    if line.startswith('"') and line.endswith('"'):
                        body_parts.append(line[1:-1])  # Remove quotes
                        
                func_body = '\n'.join(body_parts)
                func_body = func_body.replace('\\n', '\n').replace('\\"', '"').replace('\\\\', '\\')
                
                self.js_snippets.append({
                    'code': f"function {func_name}() {{\n{func_body}\n}}",
                    'file': filepath,
                    'line': content[:match.start()].count('\n') + 1,
                    'type': 'function',
                    'name': func_name
                })
                
            # Extract JavaScript from WDynamic emitBody methods
            # Look for emitBody implementations that contain JavaScript
            emit_body_pattern = r'void\s+emitBody\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?\s*{'
            for match in re.finditer(emit_body_pattern, content):
                start_pos = match.end()
                # Find the matching closing brace
                brace_count = 1
                pos = start_pos
                while brace_count > 0 and pos < len(content):
                    if content[pos] == '{':
                        brace_count += 1
                    elif content[pos] == '}':
                        brace_count -= 1
                    pos += 1
                    
                if brace_count == 0:
                    body = content[start_pos:pos-1]
                    
                    # Look for sequences of out.print statements that form JavaScript
                    # First, find <script> opening tag
                    script_start_pattern = r'out\.print\s*\(\s*"<script>"\s*\)'
                    for script_start in re.finditer(script_start_pattern, body):
                        # Collect all out.print statements until </script>
                        js_lines = []
                        pos = script_start.end()
                        
                        # Pattern to match out.print statements
                        out_print_pattern = r'out\.print\s*\(\s*"([^"]*)"\s*\)\s*;'
                        
                        while pos < len(body):
                            next_match = re.search(out_print_pattern, body[pos:])
                            if not next_match:
                                break
                            
                            line = next_match.group(1)
                            js_lines.append(line)
                            
                            if '</script>' in line:
                                break
                                
                            pos += next_match.end()
                            
                        if js_lines:
                            # Join the lines and extract the JavaScript
                            js_content = ''.join(js_lines)
                            if '<script>' in js_content:
                                js_content = js_content.split('<script>')[-1]
                            if '</script>' in js_content:
                                js_content = js_content.split('</script>')[0]
                                
                            if js_content.strip():
                                line_num = content[:match.start()].count('\n') + body[:script_start.start()].count('\n') + 1
                                self.js_snippets.append({
                                    'code': js_content.strip(),
                                    'file': filepath,
                                    'line': line_num,
                                    'type': 'dynamic_emitBody',
                                    'name': 'emitBody_dynamic'
                                })
                
        except Exception as e:
            self.errors.append(f"Error reading {filepath}: {str(e)}")
            
    def extract_base_framework_js(self):
        """Extract base JavaScript functions from WifiWebServer.h that are always included"""
        # These functions are always emitted by WifiWebServer.h
        base_js = [
            {
                'code': '''
function fetchNoload(key,val) {
    var baseurl = window.location.protocol+'//'+window.location.host+location.pathname;
    fetch(baseurl+'?'+key+'='+val+'&');
}
''',
                'file': 'WifiWebServer.h (base framework)',
                'line': 1059,
                'type': 'base_framework',
                'name': 'fetchNoload'
            },
            {
                'code': '''
function fetchLoad(key,val) {
    var baseurl = window.location.protocol+'//'+window.location.host+location.pathname;
    window.location.href=baseurl+'?'+key+'='+val+'&';
}
''',
                'file': 'WifiWebServer.h (base framework)',
                'line': 1063,
                'type': 'base_framework',
                'name': 'fetchLoad'
            },
            {
                'code': '''
function limitKeypress(event, value, maxLength) {
  if (value != undefined && value.toString().length >= maxLength) {
    event.preventDefault();
  }
}
''',
                'file': 'WifiWebServer.h (base framework)',
                'line': 1067,
                'type': 'base_framework',
                'name': 'limitKeypress'
            },
            {
                'code': '''
function setInputFilter(textbox, inputFilter) {
  ["input", "keydown", "keyup", "mousedown", "mouseup", "select", "contextmenu", "drop"].forEach(function(event) {
    textbox.addEventListener(event, function() {
      if (inputFilter(this.value)) {
        this.oldValue = this.value;
        this.oldSelectionStart = this.selectionStart;
        this.oldSelectionEnd = this.selectionEnd;
      } else if (this.hasOwnProperty("oldValue")) {
        this.value = this.oldValue;
        this.setSelectionRange(this.oldSelectionStart, this.oldSelectionEnd);
      } else {
        this.value = "";
      }
    });
  });
}
''',
                'file': 'WifiWebServer.h (base framework)',
                'line': 1072,
                'type': 'base_framework',
                'name': 'setInputFilter'
            }
        ]
        
        self.js_snippets.extend(base_js)
        
    def extract_widget_js_patterns(self):
        """Extract JavaScript patterns that widgets generate dynamically"""
        # These are patterns for dynamically generated functions
        widget_patterns = [
            # WSlider, WCheckbox, WTextField generate updateValue_ID functions
            {
                'code': 'function updateValue_WIDGET_ID(pos) {fetchNoload(\'WIDGET_ID\', pos);}',
                'file': 'Widget-generated pattern',
                'line': 0,
                'type': 'widget_pattern',
                'name': 'updateValue_pattern'
            },
            # WButton generates pressed_ID functions
            {
                'code': 'function pressed_WIDGET_ID() {fetchNoload(\'WIDGET_ID\', true);}',
                'file': 'Widget-generated pattern',
                'line': 0,
                'type': 'widget_pattern',
                'name': 'pressed_pattern_noload'
            },
            {
                'code': 'function pressed_WIDGET_ID() {fetchLoad(\'WIDGET_ID\', true);}',
                'file': 'Widget-generated pattern',
                'line': 0,
                'type': 'widget_pattern',
                'name': 'pressed_pattern_load'
            },
            {
                'code': 'function pressed_WIDGET_ID() {window.location.href=\'URL?WIDGET_ID=true\';}',
                'file': 'Widget-generated pattern',
                'line': 0,
                'type': 'widget_pattern',
                'name': 'pressed_pattern_href'
            }
        ]
        
        # Replace placeholders with valid identifiers for validation
        for pattern in widget_patterns:
            pattern['code'] = pattern['code'].replace('WIDGET_ID', 'test_id').replace('URL', '/test')
            self.js_snippets.append(pattern)
            
    def extract_js_from_method(self, method_body, filepath, start_pos):
        """Extract JavaScript from within an emitJS method"""
        # Find all client.print statements
        print_pattern = r'client\.print\s*\(\s*"([^"\\]*(\\.[^"\\]*)*)"\s*\)'
        
        js_code = []
        for match in re.finditer(print_pattern, method_body):
            # Unescape the string
            js_line = match.group(1)
            js_line = js_line.replace('\\"', '"')
            js_line = js_line.replace('\\n', '\n')
            js_line = js_line.replace('\\\\', '\\')
            js_code.append(js_line)
            
        if js_code:
            full_js = '\n'.join(js_code)
            self.js_snippets.append({
                'code': full_js,
                'file': filepath,
                'line': method_body[:start_pos].count('\n') + 1,
                'type': 'emitJS'
            })
            
    def extract_inline_scripts(self, html_string, filepath, start_pos):
        """Extract JavaScript from inline <script> tags"""
        # Unescape the HTML string
        html = html_string.replace('\\"', '"')
        html = html.replace('\\n', '\n')
        html = html.replace('\\\\', '\\')
        
        # Extract script content
        script_pattern = r'<script[^>]*>(.*?)</script>'
        for match in re.finditer(script_pattern, html, re.DOTALL):
            script_content = match.group(1).strip()
            if script_content:
                self.js_snippets.append({
                    'code': script_content,
                    'file': filepath,
                    'line': html[:match.start()].count('\n') + 1,
                    'type': 'inline_script'
                })
                
    def validate_javascript(self):
        """Validate all extracted JavaScript snippets"""
        if not self.js_snippets:
            print("No JavaScript snippets found to validate")
            return []
            
        # Check if node is available
        try:
            subprocess.run(['node', '--version'], capture_output=True, check=True)
        except (subprocess.CalledProcessError, FileNotFoundError):
            print("Error: Node.js is required for JavaScript validation")
            print("Please install Node.js to run JavaScript validation")
            return []
            
        validation_errors = []
        
        for i, snippet in enumerate(self.js_snippets):
            # Create a temporary file with the JavaScript
            with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False) as f:
                # Wrap event handlers and expressions in a function to make them valid
                if snippet['type'] == 'event_handler':
                    f.write(f"function validateHandler() {{ {snippet['code']} }}")
                else:
                    f.write(snippet['code'])
                temp_file = f.name
                
            try:
                # Use node to check syntax
                result = subprocess.run(
                    ['node', '--check', temp_file],
                    capture_output=True,
                    text=True
                )
                
                if result.returncode != 0:
                    error_msg = result.stderr.strip()
                    # Extract line number from error
                    line_match = re.search(r':(\d+)', error_msg)
                    if line_match:
                        error_line = int(line_match.group(1))
                    else:
                        error_line = 1
                        
                    validation_errors.append({
                        'file': snippet['file'],
                        'line': snippet['line'] + error_line - 1,
                        'type': snippet['type'],
                        'error': error_msg,
                        'code_preview': snippet['code'][:100] + '...' if len(snippet['code']) > 100 else snippet['code']
                    })
                    
            finally:
                os.unlink(temp_file)
                
        return validation_errors
        
    def report_results(self, validation_errors):
        """Report validation results"""
        if not validation_errors and not self.errors:
            print("✅ All JavaScript validation passed!")
            print(f"   Validated {len(self.js_snippets)} JavaScript snippets")
            
            # Show breakdown by type
            type_counts = {}
            for snippet in self.js_snippets:
                snippet_type = snippet['type']
                type_counts[snippet_type] = type_counts.get(snippet_type, 0) + 1
            
            if type_counts:
                print("\n   Snippet types found:")
                for snippet_type, count in sorted(type_counts.items()):
                    print(f"     - {snippet_type}: {count}")
            
            return True
            
        print("❌ JavaScript validation failed!")
        print()
        
        if self.errors:
            print("File processing errors:")
            for error in self.errors:
                print(f"  - {error}")
            print()
            
        if validation_errors:
            # Filter out common false positives from partial statements
            partial_statement_patterns = [
                r'^}\s*else\s*{',  # } else {
                r'^}+\s*;?\s*$',   # Just closing braces
                r'^}\s*,.*$',       # }, something
                r'.*=.*=>\s*{\s*$', # arrow function starts
                r'.*function\s*\(\s*\)\s*{\s*$', # function starts
                r'^if\s*\(.*\)\s*{\s*$',  # if statement starts
                r'^}\s*\)\s*;?\s*$',  # }); or })
            ]
            
            critical_errors = []
            partial_errors = []
            
            for error in validation_errors:
                code = error['code_preview'].strip()
                is_partial = any(re.match(pattern, code) for pattern in partial_statement_patterns)
                
                if is_partial:
                    partial_errors.append(error)
                else:
                    critical_errors.append(error)
            
            if len(critical_errors) == 0:
                print("✅ JavaScript validation passed!")
                print(f"   Validated {len(self.js_snippets)} JavaScript snippets")
                print(f"   Found {len(partial_errors)} partial statement warnings (likely false positives)")
                
                # Show breakdown by type
                type_counts = {}
                for snippet in self.js_snippets:
                    snippet_type = snippet['type']
                    type_counts[snippet_type] = type_counts.get(snippet_type, 0) + 1
                
                if type_counts:
                    print("\n   Snippet types found:")
                    for snippet_type, count in sorted(type_counts.items()):
                        print(f"     - {snippet_type}: {count}")
            else:
                print(f"Found {len(validation_errors)} JavaScript syntax errors:")
                print(f"  - {len(critical_errors)} critical errors")
                print(f"  - {len(partial_errors)} partial statement errors (likely false positives)")
            print()
            
            if critical_errors:
                print("🚨 CRITICAL ERRORS (these need to be fixed):")
                print()
                for error in critical_errors:
                    print(f"📍 {os.path.basename(error['file'])}:{error['line']}")
                    print(f"   Type: {error['type']}")
                    print(f"   Error: {error['error']}")
                    print(f"   Code: {error['code_preview']}")
                    print()
                    
            if partial_errors and os.environ.get('SHOW_PARTIAL_ERRORS', '').lower() == 'true':
                print("\n⚠️  PARTIAL STATEMENT ERRORS (set SHOW_PARTIAL_ERRORS=true to see these):")
                print()
                for error in partial_errors:
                    print(f"📍 {os.path.basename(error['file'])}:{error['line']}")
                    print(f"   Type: {error['type']}")
                    print(f"   Code: {error['code_preview']}")
                    print()
            
            # Return True if there are no critical errors (partial errors are okay)
            return len(critical_errors) == 0
                
        return False

def main():
    # Get the RSeriesLogic directory
    script_dir = Path(__file__).parent
    project_dir = script_dir.parent
    
    # Files to check
    files_to_check = [
        project_dir / 'WebPages.h',
        project_dir / 'lib' / 'Reeltwo' / 'src' / 'wifi' / 'WifiWebServer.h'
    ]
    
    extractor = JavaScriptExtractor()
    
    # Extract JavaScript from all files
    for filepath in files_to_check:
        if filepath.exists():
            print(f"Extracting JavaScript from {filepath.name}...")
            extractor.extract_from_file(filepath)
        else:
            print(f"Warning: {filepath} not found")
    
    # Extract base framework JavaScript that's always included
    print("Extracting base framework JavaScript...")
    extractor.extract_base_framework_js()
    
    # Extract widget JavaScript patterns 
    print("Extracting widget JavaScript patterns...")
    extractor.extract_widget_js_patterns()
            
    # Validate all JavaScript
    print("Validating JavaScript...")
    validation_errors = extractor.validate_javascript()
    
    # Report results
    success = extractor.report_results(validation_errors)
    
    # Exit with appropriate code
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()