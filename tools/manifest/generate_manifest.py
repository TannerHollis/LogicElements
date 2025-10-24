#!/usr/bin/env python3
"""
LogicElements Build Manifest Generator

Parses element headers and CMake configuration to generate comprehensive
JSON manifest documenting the build, all available elements, and their ports.
"""

import json
import sys
import re
import os
import argparse
from pathlib import Path
from datetime import datetime


def parse_element_enum(header_path):
    """
    Parse le_Element.hpp and extract element type enum.
    
    Returns dict: {element_name: {"id": int, "description": str, "heterogeneous": bool}}
    """
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Find enum class ElementType { ... };
    enum_match = re.search(r'enum\s+class\s+ElementType[^{]*\{(.*?)\};', content, re.DOTALL)
    if not enum_match:
        print("ERROR: Could not find ElementType enum in", header_path, file=sys.stderr)
        return {}
    
    enum_content = enum_match.group(1)
    
    # Parse: ElementName = ID, ///< Description
    pattern = r'(\w+)\s*=\s*(-?\d+),?\s*///< (.+?)(?=\n|$)'
    matches = re.findall(pattern, enum_content, re.MULTILINE)
    
    element_info = {}
    for name, id_val, desc in matches:
        # Check for HETEROGENEOUS marker
        is_heterogeneous = "[HETEROGENEOUS]" in desc or "HETEROGENEOUS!" in desc
        
        # Clean description
        desc_clean = desc.replace("[HETEROGENEOUS]", "").replace("HETEROGENEOUS!", "")
        desc_clean = desc_clean.replace("[HETEROGENEOUS!", "").strip()
        
        element_info[name] = {
            "id": int(id_val),
            "description": desc_clean,
            "heterogeneous": is_heterogeneous
        }
    
    return element_info


def find_element_files(base_dir, element_name):
    """Find the header AND source files for a given element type."""
    # Handle Mux template specializations
    # MuxDigital, MuxAnalog, MuxAnalogComplex all use le_Mux.hpp/.cpp
    search_name = element_name
    if element_name.startswith("Mux"):
        search_name = "Mux"
    
    # Element name to file name mapping
    file_patterns = [
        f"le_{search_name}.hpp",
        f"le_{search_name}.cpp",
        f"le_{search_name.lower()}.hpp",
        f"le_{search_name.lower()}.cpp",
    ]
    
    found_files = []
    
    # Search in all subdirectories
    for root, dirs, files in os.walk(base_dir):
        for pattern in file_patterns:
            if pattern.lower() in [f.lower() for f in files]:
                # Find exact match
                for f in files:
                    if f.lower() == pattern.lower():
                        found_files.append(os.path.join(root, f))
    
    return found_files


def parse_constructor_arguments(args_text):
    """
    Parse constructor arguments and extract descriptions.
    
    Args:
        args_text: String containing constructor arguments like "uint16_t arg1, ///< Description"
    
    Returns:
        List of {"name": str, "type": str, "description": str}
    """
    args = []
    
    # Split by comma, but be careful with template parameters
    # Simple approach: split by comma and clean up
    arg_parts = []
    current_arg = ""
    template_depth = 0
    
    for char in args_text:
        if char == '<':
            template_depth += 1
        elif char == '>':
            template_depth -= 1
        elif char == ',' and template_depth == 0:
            arg_parts.append(current_arg.strip())
            current_arg = ""
            continue
        current_arg += char
    
    if current_arg.strip():
        arg_parts.append(current_arg.strip())
    
    for arg_text in arg_parts:
        arg_text = arg_text.strip()
        if not arg_text:
            continue
            
        # Look for description: ///< Description
        desc_match = re.search(r'///<\s*(.+)$', arg_text)
        description = desc_match.group(1).strip() if desc_match else ""
        
        # Remove description from arg_text
        arg_text = re.sub(r'///<.*$', '', arg_text).strip()
        
        # Extract type and name: "uint16_t argName" or "float argName = 0.0f"
        # Handle complex types like "std::complex<float>"
        # Also handle default values: "uint8_t argName = 3"
        type_name_match = re.match(r'([^=\s]+(?:\s*<[^>]*>)?)\s+(\w+)(?:\s*=\s*[^,]*)?', arg_text)
        if type_name_match:
            arg_type = type_name_match.group(1).strip()
            arg_name = type_name_match.group(2).strip()
            
            # Simplify type names
            arg_type = simplify_type(arg_type)
            
            args.append({
                "name": arg_name,
                "type": arg_type,
                "description": description
            })
    
    return args


def parse_element_ports(element_files, element_name):
    """
    Parse element files (header and source) to extract port definitions.
    
    Returns: {"inputs": [...], "outputs": [...]}
    """
    # Combine content from all files (header + source)
    content = ""
    for file_path in element_files:
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content += f.read() + "\n"
        except Exception as e:
            print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
    
    ports = {"inputs": [], "outputs": []}
    constructor_args = []
    
    # Extract constructor arguments with descriptions
    # Look for constructor definitions in both header and source files
    constructor_patterns = [
        # Pattern: ConstructorName(args) - more flexible matching
        (r'(\w+)\s*\(\s*([^)]*)\s*\)\s*;', False),  # Header declaration (most common)
        (r'(\w+)\s*\(\s*([^)]*)\s*\)\s*\{', False),  # Constructor implementation
        (r'(\w+)\s*\(\s*([^)]*)\s*\)\s*:\s*Element\s*\([^)]*\)\s*\{', True),  # Constructor with initializer list
    ]
    
    found_constructor = False
    for pattern, has_initializer in constructor_patterns:
        if found_constructor:
            break
        for match in re.finditer(pattern, content, re.MULTILINE):
            constructor_name = match.group(1)
            if constructor_name == element_name:  # Only match the main constructor
                args_text = match.group(2)
                if args_text.strip():  # Only process if there are arguments
                    args = parse_constructor_arguments(args_text)
                    constructor_args.extend(args)
                found_constructor = True
                break  # Found constructor, stop searching
    
    # Pattern: AddInputPort<TYPE>("name") or AddInputPort<TYPE>(LE_PORT_INPUT_NAME(N))
    # Also handle: pInput = AddInputPort<TYPE>(...)
    input_patterns = [
        (r'AddInputPort<([^>]+)>\s*\(\s*"([^"]+)"\s*\)', False),  # Literal name
        (r'AddInputPort<([^>]+)>\s*\(\s*LE_PORT_INPUT_NAME\s*\(\s*(\d+)\s*\)\s*\)', True),  # Indexed
        (r'AddInputPort<([^>]+)>\s*\(\s*LE_PORT_MATH_VAR_NAME\s*\(\s*(\d+)\s*\)\s*\)', "math"),  # Math var
        (r'AddInputPort<([^>]+)>\s*\(\s*LE_PORT_INPUT_PREFIX\s*\)', "single_input"),  # Single input (Node)
    ]
    
    output_patterns = [
        (r'AddOutputPort<([^>]+)>\s*\(\s*"([^"]+)"\s*\)', False),  # Literal name
        (r'AddOutputPort<([^>]+)>\s*\(\s*LE_PORT_OUTPUT_NAME\s*\(\s*(\d+)\s*\)\s*\)', True),  # Indexed
        (r'AddOutputPort<([^>]+)>\s*\(\s*LE_PORT_OUTPUT_PREFIX\s*\)', "single"),  # Single output
    ]
    
    # Parse inputs
    for pattern, indexed_type in input_patterns:
        for match in re.finditer(pattern, content):
            port_type_cpp = match.group(1).strip()
            port_type = simplify_type(port_type_cpp)
            
            if indexed_type == True:
                # Indexed: input_0, input_1, etc.
                port_name = f"input_{match.group(2)}"
                indexed = True
            elif indexed_type == "math":
                # Math variable: x0, x1, etc.
                port_name = f"x{match.group(2)}"
                indexed = True
            elif indexed_type == "single_input":
                # Single input (Node elements)
                port_name = "input"
                indexed = False
            else:
                # Literal name
                port_name = match.group(2)
                indexed = False
            
            ports["inputs"].append({
                "name": port_name,
                "type": port_type,
                "indexed": indexed
            })
    
    # Parse outputs
    for pattern, indexed_type in output_patterns:
        for match in re.finditer(pattern, content):
            port_type_cpp = match.group(1).strip()
            port_type = simplify_type(port_type_cpp)
            
            if indexed_type == True:
                port_name = f"output_{match.group(2)}"
                indexed = True
            elif indexed_type == "single":
                port_name = "output"
                indexed = False
            else:
                port_name = match.group(2)
                indexed = False
            
            ports["outputs"].append({
                "name": port_name,
                "type": port_type,
                "indexed": indexed
            })
    
    # Deduplicate ports (same name/type)
    ports["inputs"] = deduplicate_ports(ports["inputs"])
    ports["outputs"] = deduplicate_ports(ports["outputs"])
    
    # Check if ports are created dynamically (in loops)
    # Look for patterns like: for(...) { AddInputPort }
    dynamic_inputs = False
    dynamic_outputs = False
    
    if re.search(r'for\s*\([^)]*\)\s*\{[^}]*AddInputPort', content, re.DOTALL):
        dynamic_inputs = True
    if re.search(r'for\s*\([^)]*\)\s*\{[^}]*AddOutputPort', content, re.DOTALL):
        dynamic_outputs = True
    
    # Add metadata if ports are dynamic
    if dynamic_inputs and not ports["inputs"]:
        ports["inputs_note"] = "Dynamically created based on constructor arguments"
    if dynamic_outputs and not ports["outputs"]:
        ports["outputs_note"] = "Dynamically created based on constructor arguments"
    
    return {"ports": ports, "constructor_args": constructor_args}


def deduplicate_ports(port_list):
    """Remove duplicate port definitions."""
    seen = set()
    unique_ports = []
    
    for port in port_list:
        key = (port["name"], port["type"])
        if key not in seen:
            seen.add(key)
            unique_ports.append(port)
    
    return unique_ports


def simplify_type(cpp_type):
    """Convert C++ types to simple manifest types."""
    cpp_type = cpp_type.strip()
    
    type_map = {
        "bool": "bool",
        "float": "float",
        "std::complex<float>": "complex",
        "std::complex<float> ": "complex",
    }
    
    return type_map.get(cpp_type, cpp_type)


def categorize_element(element_id):
    """Categorize element by ID range."""
    if element_id == 0 or element_id in [50, 51]:
        return "node"
    elif 10 <= element_id <= 19:
        return "digital"
    elif 30 <= element_id <= 49:
        return "digital"
    elif 60 <= element_id <= 68:
        return "conversions"
    elif 69 <= element_id <= 79 or element_id == 84:
        return "arithmetic"
    elif 80 <= element_id <= 83:
        return "control"
    elif 62 <= element_id <= 62:
        return "power"
    elif 81 <= element_id <= 82:
        return "power"
    elif 100 <= element_id <= 119:
        return "protection"
    else:
        return "other"


def generate_manifest(args):
    """Generate complete manifest JSON."""
    
    # Parse element enum
    element_info = parse_element_enum(args.element_header)
    
    if not element_info:
        print("ERROR: No elements found in enum", file=sys.stderr)
        return None
    
    # Build element list with port information
    elements = []
    base_dir = Path(args.element_header).parent.parent.parent.parent  # Back to src/LogicElements root
    
    for element_name, info in element_info.items():
        # Skip Invalid
        if element_name == "Invalid":
            continue
        
        # Find element files (both .hpp and .cpp)
        element_files = find_element_files(base_dir, element_name)
        
        # Parse ports and constructor arguments from files
        if element_files:
            element_data = parse_element_ports(element_files, element_name)
            ports = element_data["ports"]
            constructor_args = element_data["constructor_args"]
        else:
            ports = {"inputs": [], "outputs": []}
            constructor_args = []
            print(f"Warning: Could not find files for {element_name}", file=sys.stderr)
        
        # Categorize
        category = categorize_element(info["id"])
        
        elements.append({
            "type": element_name,
            "id": info["id"],
            "description": info["description"],
            "category": category,
            "heterogeneous": info["heterogeneous"],
            "ports": ports,
            "constructor_args": constructor_args
        })
    
    # Sort by ID
    elements.sort(key=lambda x: x["id"])
    
    # Build manifest
    manifest = {
        "library": {
            "name": args.project_name,
            "version": args.version,
            "output": args.library_output,
            "buildType": args.build_type,
            "buildDate": datetime.now().strftime("%Y-%m-%d"),
            "buildTime": datetime.now().strftime("%H:%M:%S"),
            "cxxStandard": args.cxx_standard,
            "platform": {
                "host": args.host_system,
                "compiler": args.compiler,
                "boardHAL": args.board_platform
            }
        },
        "features": {
            "testMode": args.test_mode == "ON",
            "executionDiag": args.execution_diag == "ON",
            "analogElements": args.analog == "ON",
            "analogComplex": args.analog_complex == "ON",
            "pidController": args.pid == "ON",
            "mathEvaluator": args.math == "ON",
            "dnp3Protocol": args.dnp3 == "ON"
        },
        "configuration": {
            "constants": {
                "elementNameLength": 16,
                "engineNameLength": 32,
                "elementArgumentLength": 64
            },
            "portNaming": {
                "inputPrefix": "input",
                "outputPrefix": "output",
                "separator": "_",
                "mathVarPrefix": "x",
                "selectorName": "selector"
            }
        },
        "elements": elements,
        "statistics": {
            "totalElements": len(elements),
            "heterogeneousElements": sum(1 for e in elements if e["heterogeneous"]),
            "digitalElements": sum(1 for e in elements if e["category"] == "digital"),
            "arithmeticElements": sum(1 for e in elements if e["category"] == "arithmetic"),
            "sourceFiles": int(args.source_count) if args.source_count else 0,
            "headerFiles": int(args.header_count) if args.header_count else 0,
            "testFiles": 34
        }
    }
    
    return manifest


def main():
    parser = argparse.ArgumentParser(description='Generate LogicElements build manifest')
    parser.add_argument('--output', required=True, help='Output JSON file path')
    parser.add_argument('--element-header', required=True, help='Path to le_Element.hpp')
    parser.add_argument('--project-name', default='Logic_Elements', help='Project name')
    parser.add_argument('--version', default='2.0.0', help='Library version')
    parser.add_argument('--library-output', default='', help='Library output path')
    parser.add_argument('--build-type', default='Release', help='Build type')
    parser.add_argument('--cxx-standard', default='17', help='C++ standard')
    parser.add_argument('--host-system', default='Unknown', help='Host system name')
    parser.add_argument('--compiler', default='Unknown', help='Compiler ID and version')
    parser.add_argument('--board-platform', default='Generic', help='Board HAL platform')
    parser.add_argument('--test-mode', default='ON', help='Test mode enabled')
    parser.add_argument('--execution-diag', default='ON', help='Execution diagnostics')
    parser.add_argument('--analog', default='ON', help='Analog elements enabled')
    parser.add_argument('--analog-complex', default='ON', help='Complex analog enabled')
    parser.add_argument('--pid', default='ON', help='PID enabled')
    parser.add_argument('--math', default='ON', help='Math enabled')
    parser.add_argument('--dnp3', default='ON', help='DNP3 enabled')
    parser.add_argument('--source-count', default='0', help='Number of source files')
    parser.add_argument('--header-count', default='0', help='Number of header files')
    
    args = parser.parse_args()
    
    # Generate manifest
    manifest = generate_manifest(args)
    
    if manifest is None:
        print("ERROR: Failed to generate manifest", file=sys.stderr)
        return 1
    
    # Write to output file
    with open(args.output, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
    
    print(f"[OK] Manifest generated: {args.output}")
    print(f"  - {len(manifest['elements'])} elements documented")
    print(f"  - {manifest['statistics']['heterogeneousElements']} heterogeneous elements")
    print(f"  - Platform: {manifest['library']['platform']['boardHAL']}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())

