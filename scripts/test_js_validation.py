#!/usr/bin/env python3
"""
Test the JavaScript validation script to ensure it catches common errors
"""

import subprocess
import tempfile
import os

# Test cases with known JavaScript errors
test_cases = [
    {
        'name': 'Missing semicolon',
        'code': 'var x = 5\nvar y = 6',
        'should_fail': False  # Modern JS doesn't require semicolons
    },
    {
        'name': 'Undefined function call',
        'code': 'undefinedFunction();',
        'should_fail': False  # Syntax is valid, runtime error
    },
    {
        'name': 'Invalid syntax - missing closing brace',
        'code': 'function test() { console.log("test")',
        'should_fail': True
    },
    {
        'name': 'Invalid syntax - extra closing brace',
        'code': 'function test() { console.log("test"); }}',
        'should_fail': True
    },
    {
        'name': 'String concatenation error',
        'code': 'var msg = "Hello" + ssid + "";',
        'should_fail': False  # Valid syntax, undefined variable
    },
    {
        'name': 'Missing quotes in string',
        'code': 'var msg = Hello World;',
        'should_fail': True
    },
    {
        'name': 'Incomplete function call',
        'code': 'pressed_addnet(',
        'should_fail': True
    },
    {
        'name': 'Valid function reference',
        'code': 'pressed_addnet',
        'should_fail': False
    }
]

def test_javascript(code):
    """Test if JavaScript code is syntactically valid using Node.js"""
    with tempfile.NamedTemporaryFile(mode='w', suffix='.js', delete=False) as f:
        f.write(code)
        temp_file = f.name
        
    try:
        result = subprocess.run(
            ['node', '--check', temp_file],
            capture_output=True,
            text=True
        )
        return result.returncode == 0, result.stderr
    finally:
        os.unlink(temp_file)

def main():
    print("Testing JavaScript validation...")
    print("-" * 60)
    
    for test in test_cases:
        is_valid, error = test_javascript(test['code'])
        expected = not test['should_fail']
        
        if is_valid == expected:
            status = "✅ PASS"
        else:
            status = "❌ FAIL"
            
        print(f"{status} {test['name']}")
        if not is_valid:
            print(f"     Error: {error.strip()}")
        print()

if __name__ == '__main__':
    main()