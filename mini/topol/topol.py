#!/usr/bin/env python3
"""
ARPANET 1973 Network Topology Visualizer
Reads network data from arpanetNodes array and displays the network graphically
"""

import re
import os
import glob
import zipfile
import networkx as nx
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

def parse_arpanet_nodes(js_file_path):
    """
    Parse the arpanetNodes JavaScript array and extract IMP nodes with connections
    Returns a list of dictionaries containing node data
    """
    with open(js_file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract the arpanetNodes array
    match = re.search(r'const arpanetNodes = \[(.*?)\];', content, re.DOTALL)
    if not match:
        raise ValueError("Could not find arpanetNodes array in file")

    array_content = match.group(1)

    # Parse individual node objects - only those with 'node:' or 'node' property
    nodes = []
    # Match objects that have a 'node:' or 'node ' property (with or without colon)
    node_pattern = r'\{\s*node\s*:?\s*(\d+)\s*,([^}]+)\}'

    for match in re.finditer(node_pattern, array_content):
        node_num = int(match.group(1))
        props = match.group(2)

        node_data = {'node': node_num}

        # Extract modem connections (handle variations like 'modem1', 'modem 1', 'modem2:', etc.)
        modem1_match = re.search(r'modem\s*1\s*:\s*(\d+)', props)
        modem2_match = re.search(r'modem\s*2\s*:\s*(\d+)', props)
        modem3_match = re.search(r'modem\s*3\s*:\s*(\d+)', props)

        if modem1_match:
            node_data['modem1'] = int(modem1_match.group(1))
        if modem2_match:
            node_data['modem2'] = int(modem2_match.group(1))
        if modem3_match:
            node_data['modem3'] = int(modem3_match.group(1))

        # Extract name1 (location name)
        name1_match = re.search(r"name1\s*:\s*['\"]([^'\"]+)['\"]", props)
        if name1_match:
            node_data['name1'] = name1_match.group(1)
        else:
            node_data['name1'] = f"Node-{node_num}"

        # Extract x and y coordinates if present
        x_match = re.search(r'x\s*:\s*(\d+(?:\.\d+)?)', props)
        y_match = re.search(r'y\s*:\s*(\d+(?:\.\d+)?)', props)
        if x_match:
            node_data['x'] = float(x_match.group(1))
        if y_match:
            node_data['y'] = float(y_match.group(1))

        nodes.append(node_data)

    return nodes

def parse_host_computers(js_file_path):
    """
    Parse host computers (entries without 'node:' property) from the arpanetNodes array
    Returns a dictionary mapping location names to lists of hosts
    """
    with open(js_file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract the arpanetNodes array
    match = re.search(r'const arpanetNodes = \[(.*?)\];', content, re.DOTALL)
    if not match:
        return {}

    array_content = match.group(1)

    # Find all object entries
    hosts_by_location = {}

    # Match objects that do NOT have a 'node:' property but have 'name1' and 'host'
    # Look for entries with host: but without node:
    for obj_match in re.finditer(r'\{([^}]+)\}', array_content):
        obj_content = obj_match.group(1)

        # Skip if it has a 'node:' property
        if re.search(r'\bnode\s*:?\s*\d+', obj_content):
            continue

        # Check if it has host and name1
        host_match = re.search(r'host\s*:\s*(\d+)', obj_content)
        name1_match = re.search(r"name1\s*:\s*['\"]([^'\"]+)['\"]", obj_content)
        hostname_match = re.search(r"hostname\s*:\s*['\"]([^'\"]+)['\"]", obj_content)
        front_match = re.search(r'front\s*:\s*(\d+)', obj_content)

        if host_match and name1_match:
            location = name1_match.group(1)
            host_num = int(host_match.group(1))
            hostname = hostname_match.group(1) if hostname_match else None
            front = int(front_match.group(1)) if front_match else 0

            if location not in hosts_by_location:
                hosts_by_location[location] = []

            hosts_by_location[location].append({
                'host': host_num,
                'hostname': hostname,
                'front': front
            })

    return hosts_by_location

def parse_pdp_hosts_file():
    """
    Search for and parse pdp-hosts configuration file.
    Searches in: ./ then ../ then ../../
    Returns: set of (imp_num, host_index) tuples that should have convert mode
    """
    search_paths = ['.', '..', '../..']
    pdp_hosts = set()
    found_file = None

    for search_dir in search_paths:
        pdp_hosts_path = os.path.join(search_dir, 'pdp-hosts')
        if os.path.exists(pdp_hosts_path):
            found_file = os.path.abspath(pdp_hosts_path)
            print(f"\n   Using pdp-hosts file: {found_file}")

            with open(pdp_hosts_path, 'r') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()

                    # Skip comments and empty lines
                    if not line or line.startswith('#'):
                        continue

                    # Parse: imp <number>, host <index>
                    match = re.match(r'imp\s+(\d+)\s*,\s*host\s+(\d+)', line, re.IGNORECASE)
                    if match:
                        imp_num = int(match.group(1))
                        host_index = int(match.group(2))
                        pdp_hosts.add((imp_num, host_index))
                        print(f"      - IMP {imp_num:02d}, host {host_index} will get convert mode")
                    else:
                        print(f"   WARNING: Line {line_num} in pdp-hosts has invalid format: {line}")

            break

    if not found_file:
        print("\n   No pdp-hosts file found (searched: ./, ../, ../../)")
        print("   Convert mode will NOT be added to any host interfaces")

    return pdp_hosts

def validate_connections(nodes):
    """
    Validate that all modem connections are bidirectional
    Returns a list of errors found
    """
    errors = []

    # Build a map of node number to its modem connections
    node_modems = {}
    node_names = {}

    for node in nodes:
        node_num = node['node']
        node_names[node_num] = node['name1']
        modems = []
        for modem_key in ['modem1', 'modem2', 'modem3']:
            if modem_key in node:
                modems.append(node[modem_key])
        node_modems[node_num] = modems

    # Check each connection is bidirectional
    for node_num, modems in node_modems.items():
        node_name = node_names.get(node_num, f"Node-{node_num}")

        for target in modems:
            # Check if target node exists
            if target not in node_modems:
                errors.append(f"  ⚠ {node_name} (node {node_num}) connects to non-existent node {target}")
                continue

            # Check if target connects back
            if node_num not in node_modems[target]:
                target_name = node_names.get(target, f"Node-{target}")
                errors.append(
                    f"  ⚠ {node_name} (node {node_num}) → {target_name} (node {target}), "
                    f"but {target_name} does not connect back"
                )

    return errors

def build_network_graph(nodes):
    """
    Build a NetworkX graph from the parsed nodes
    Returns the graph and a mapping of node numbers to names
    """
    G = nx.Graph()
    node_names = {}

    # Add nodes
    for node in nodes:
        node_num = node['node']
        node_name = node['name1']
        G.add_node(node_num)
        node_names[node_num] = node_name

    # Add edges based on modem connections
    for node in nodes:
        node_num = node['node']
        for modem_key in ['modem1', 'modem2', 'modem3']:
            if modem_key in node:
                target = node[modem_key]
                if target in node_names:  # Only add edge if target node exists
                    G.add_edge(node_num, target)

    return G, node_names

def create_fixed_layout(G, node_names, nodes_data):
    """
    Create a layout with fixed positions for specific nodes and using original x,y coordinates:
    - SRI (top left) - manually fixed
    - MIT (top right) - manually fixed
    - ETAC (bottom right) - manually fixed
    - USC-ISI (bottom left) - manually fixed
    - DOCB (center) - manually fixed
    - Other nodes: use original x,y coordinates if available, otherwise spring layout

    Returns a position dictionary for all nodes
    """
    # Build a map of node number to original coordinates
    node_coords = {}
    for node_data in nodes_data:
        if 'x' in node_data and 'y' in node_data:
            node_coords[node_data['node']] = (node_data['x'], node_data['y'])

    # Define fixed positions (normalized coordinates)
    fixed_positions = {}
    fixed_nodes = set()

    # Find node numbers by name
    name_to_node = {name: num for num, name in node_names.items()}

    # Set initial fixed positions
    if 'SRI' in name_to_node:
        fixed_positions[name_to_node['SRI']] = (-10.0, 10.0)  # Top left
        fixed_nodes.add(name_to_node['SRI'])
        print(f"   - Fixed SRI (node {name_to_node['SRI']}) at top left")

    if 'MIT' in name_to_node:
        fixed_positions[name_to_node['MIT']] = (10.0, 10.0)   # Top right
        fixed_nodes.add(name_to_node['MIT'])
        print(f"   - Fixed MIT (node {name_to_node['MIT']}) at top right")

    if 'ETAC' in name_to_node:
        fixed_positions[name_to_node['ETAC']] = (10.0, -10.0) # Bottom right
        fixed_nodes.add(name_to_node['ETAC'])
        print(f"   - Fixed ETAC (node {name_to_node['ETAC']}) at bottom right")

    if 'USC-ISI' in name_to_node:
        fixed_positions[name_to_node['USC-ISI']] = (-10.0, -10.0) # Bottom left
        fixed_nodes.add(name_to_node['USC-ISI'])
        print(f"   - Fixed USC-ISI (node {name_to_node['USC-ISI']}) at bottom left")

    if 'DOCB' in name_to_node:
        fixed_positions[name_to_node['DOCB']] = (0.0, 0.0)  # Center
        fixed_nodes.add(name_to_node['DOCB'])
        print(f"   - Fixed DOCB (node {name_to_node['DOCB']}) at center")

    # For non-fixed nodes, use original coordinates if available
    non_fixed_nodes = [n for n in G.nodes() if n not in fixed_nodes]

    # Separate nodes with and without coordinates
    nodes_with_coords = [n for n in non_fixed_nodes if n in node_coords]
    nodes_without_coords = [n for n in non_fixed_nodes if n not in node_coords]

    print(f"   - {len(nodes_with_coords)} nodes using original x,y coordinates")
    print(f"   - {len(nodes_without_coords)} nodes using spring layout")

    # Add original coordinates to fixed_positions (but not to fixed_nodes)
    if nodes_with_coords:
        # Get bounds of original coordinates to scale them
        orig_x_vals = [node_coords[n][0] for n in nodes_with_coords]
        orig_y_vals = [node_coords[n][1] for n in nodes_with_coords]

        orig_x_min, orig_x_max = min(orig_x_vals), max(orig_x_vals)
        orig_y_min, orig_y_max = min(orig_y_vals), max(orig_y_vals)

        # Scale original coordinates to fit within (-8, 8) range
        for node in nodes_with_coords:
            x_orig, y_orig = node_coords[node]
            # Normalize and scale
            x_norm = (x_orig - orig_x_min) / (orig_x_max - orig_x_min) if orig_x_max != orig_x_min else 0.5
            y_norm = (y_orig - orig_y_min) / (orig_y_max - orig_y_min) if orig_y_max != orig_y_min else 0.5
            # Invert y (because screen coordinates are top-down, graph is bottom-up)
            fixed_positions[node] = (x_norm * 16 - 8, -(y_norm * 16 - 8))

    # Use spring layout only for nodes without coordinates
    if nodes_without_coords:
        pos = nx.spring_layout(
            G,
            pos=fixed_positions,
            fixed=list(fixed_nodes) + nodes_with_coords,  # Fix both manual and coord-based nodes
            k=1.5,
            iterations=300,
            seed=42
        )
    else:
        # All nodes have positions, no need for spring layout
        pos = fixed_positions.copy()

    # FORCE the manually fixed positions (ensure they don't move)
    for node in fixed_nodes:
        pos[node] = fixed_positions[node]

    # FORCE the coordinate-based positions
    for node in nodes_with_coords:
        if node in fixed_positions:
            pos[node] = fixed_positions[node]

    return pos

def visualize_network(G, pos, node_names, hosts_by_location=None, node_to_location=None):
    """
    Visualize the network using matplotlib with host information
    """
    plt.figure(figsize=(20, 14))
    plt.title('ARPANET Network Topology (1973)', fontsize=20, fontweight='bold', pad=20)

    # Create reverse mapping: location name -> node number
    location_to_node = {name: num for num, name in node_names.items()}

    # Draw edges first (so they appear behind nodes)
    nx.draw_networkx_edges(
        G, pos,
        edge_color='#666666',
        width=2,
        alpha=0.6
    )

    # Prepare node colors - highlight the fixed nodes
    fixed_node_names = {'SRI', 'MIT', 'ETAC', 'USC-ISI', 'DOCB'}
    node_colors = []
    for node in G.nodes():
        if node_names[node] in fixed_node_names:
            node_colors.append('#FF6B6B')  # Red for fixed nodes
        else:
            node_colors.append('#4ECDC4')  # Teal for other nodes

    # Draw nodes
    nx.draw_networkx_nodes(
        G, pos,
        node_color=node_colors,
        node_size=800,
        alpha=0.9,
        edgecolors='#333333',
        linewidths=2
    )

    # Draw labels with node names
    labels = {node: node_names[node] for node in G.nodes()}
    nx.draw_networkx_labels(
        G, pos,
        labels,
        font_size=9,
        font_weight='bold',
        font_color='#FFFFFF'
    )

    # Add legend
    from matplotlib.patches import Patch
    legend_elements = [
        Patch(facecolor='#FF6B6B', edgecolor='#333333', label='Fixed Position Nodes'),
        Patch(facecolor='#4ECDC4', edgecolor='#333333', label='Other Nodes')
    ]
    plt.legend(handles=legend_elements, loc='upper left', fontsize=12)

    # Add annotations for fixed nodes
    name_to_node = {name: num for num, name in node_names.items()}
    annotations = [
        ('SRI', 'Top Left'),
        ('MIT', 'Top Right'),
        ('ETAC', 'Bottom Right'),
        ('USC-ISI', 'Bottom Left'),
        ('DOCB', 'Center')
    ]

    for node_name, position_label in annotations:
        if node_name in name_to_node:
            node_num = name_to_node[node_name]
            x, y = pos[node_num]
            plt.annotate(
                f'({position_label})',
                xy=(x, y),
                xytext=(10, -15),
                textcoords='offset points',
                fontsize=8,
                style='italic',
                color='#666666'
            )

    # Add host information as text labels near nodes
    if hosts_by_location:
        for node in G.nodes():
            node_name = node_names[node]

            # Check if this location has hosts
            if node_name in hosts_by_location:
                hosts = hosts_by_location[node_name]

                if hosts:
                    # Get node position
                    x, y = pos[node]

                    # Get IMP number for this location
                    imp_number = location_to_node.get(node_name, 0)

                    # Build host text
                    host_lines = []
                    for host_info in hosts:
                        host_num = host_info['host']
                        # Calculate: (host_number - imp_number) / 64
                        display_host_num = int((host_num - imp_number) / 64)

                        if host_info['hostname']:
                            host_lines.append(f"  host: {display_host_num} ({host_info['hostname']})")
                        else:
                            host_lines.append(f"  host: {display_host_num}")

                    host_text = '\n'.join(host_lines)

                    # Position text to the right and slightly below the node
                    # Adjust offset based on position to avoid edges of graph
                    x_offset = 0.8 if x < 5 else -0.8  # Right for left nodes, left for right nodes
                    y_offset = -0.3

                    text_x = x + x_offset
                    text_y = y + y_offset

                    # Draw leader line from node to text box
                    # Connect from node edge to text box
                    plt.plot(
                        [x, text_x],
                        [y, text_y],
                        color='#FF0000',
                        linewidth=1.5,
                        linestyle='--',
                        alpha=0.7,
                        zorder=1  # Draw behind text
                    )

                    plt.text(
                        text_x, text_y,
                        host_text,
                        fontsize=7,
                        color='#555555',
                        verticalalignment='top',
                        horizontalalignment='left' if x < 5 else 'right',
                        bbox=dict(boxstyle='round,pad=0.3', facecolor='white', edgecolor='#CCCCCC', alpha=0.8),
                        zorder=2  # Draw in front of leader line
                    )

    plt.axis('off')
    plt.tight_layout()
    plt.show()

def parse_simh_config(config_file):
    """
    Parse a SIMH config file to extract IMP number and modem connections
    Returns dict with imp_num and modems list
    """
    try:
        with open(config_file, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        return None

    config = {
        'imp_num': None,
        'modems': {}  # modem_num -> {'local_port': X, 'remote_port': Y}
    }

    for line in lines:
        line = line.strip()

        # Parse IMP number
        if line.startswith('set imp num='):
            config['imp_num'] = int(line.split('=')[1])

        # Parse modem interface: attach -u mi1 11102::11116
        if line.startswith('attach -u mi'):
            parts = line.split()
            if len(parts) >= 3:
                modem_interface = parts[2]  # mi1, mi2, mi3
                modem_num = int(modem_interface[-1])

                ports = parts[3]  # 11102::11116
                if '::' in ports:
                    local_port, remote_port = ports.split('::')
                    config['modems'][modem_num] = {
                        'local_port': local_port,
                        'remote_port': remote_port
                    }

    return config

def verify_simh_configs(nodes_data, config_dir='..'):
    """
    Verify all SIMH config files against the network data
    Returns list of errors found
    """
    errors = []

    # Build expected connections from network data
    expected_modems = {}  # imp_num -> {modem_num -> remote_imp}
    for node in nodes_data:
        imp_num = node['node']
        expected_modems[imp_num] = {}
        for modem_key in ['modem1', 'modem2', 'modem3']:
            if modem_key in node:
                modem_num = int(modem_key[-1])
                expected_modems[imp_num][modem_num] = node[modem_key]

    # Find all IMP config files (imp[0-9][0-9].simh only, not impcode.simh or impconfig.simh)
    config_files = glob.glob(os.path.join(config_dir, 'imp[0-9][0-9].simh'))

    if not config_files:
        errors.append("No SIMH config files found to verify")
        return errors

    print(f"   Found {len(config_files)} config files to verify...")

    # Parse and verify each config
    for config_file in sorted(config_files):
        config = parse_simh_config(config_file)
        if not config or config['imp_num'] is None:
            errors.append(f"Could not parse {os.path.basename(config_file)}")
            continue

        imp_num = config['imp_num']
        imp_num_str = f"{imp_num:02d}"

        # Check if this IMP exists in network data
        if imp_num not in expected_modems:
            errors.append(f"IMP {imp_num:02d}: Not defined in network data")
            continue

        expected = expected_modems[imp_num]

        # Verify each modem connection
        for modem_num in [1, 2, 3]:
            if modem_num in expected:
                remote_imp = expected[modem_num]
                remote_imp_str = f"{remote_imp:02d}"

                # Find which modem on remote IMP connects back
                remote_modem = None
                if remote_imp in expected_modems:
                    for rm, target in expected_modems[remote_imp].items():
                        if target == imp_num:
                            remote_modem = rm
                            break

                if not remote_modem:
                    errors.append(f"IMP {imp_num_str} modem{modem_num}: Remote IMP {remote_imp_str} doesn't connect back")
                    continue

                # Calculate expected ports: 11[m][ii] format (5 digits, bidirectional)
                expected_local_port = f"11{modem_num}{imp_num_str}"
                expected_remote_port = f"11{remote_modem}{remote_imp_str}"

                # Check if modem is configured
                if modem_num not in config['modems']:
                    errors.append(f"IMP {imp_num_str} modem{modem_num}: Missing configuration (should connect to IMP {remote_imp_str})")
                    continue

                actual = config['modems'][modem_num]

                # Verify ports
                if actual['local_port'] != expected_local_port:
                    errors.append(f"IMP {imp_num_str} modem{modem_num}: Wrong local port (got {actual['local_port']}, expected {expected_local_port})")

                if actual['remote_port'] != expected_remote_port:
                    errors.append(f"IMP {imp_num_str} modem{modem_num}: Wrong remote port (got {actual['remote_port']}, expected {expected_remote_port})")

            elif modem_num in config['modems']:
                # Modem configured but not expected
                errors.append(f"IMP {imp_num_str} modem{modem_num}: Configured but not defined in network data")

    return errors

def generate_imp_config(imp_node, nodes_data, node_names, hosts_by_location, pdp_hosts, output_dir='..'):
    """
    Generate SIMH configuration file for a single IMP
    """
    imp_num = imp_node['node']
    imp_name = imp_node['name1']
    imp_num_str = f"{imp_num:02d}"

    # Find which modems on remote IMPs connect back to this IMP
    # Build a map of node_num -> modem connections
    node_modems = {}
    for node in nodes_data:
        node_num = node['node']
        node_modems[node_num] = {}
        for modem_key in ['modem1', 'modem2', 'modem3']:
            if modem_key in node:
                modem_num = int(modem_key[-1])  # Extract 1, 2, or 3
                node_modems[node_num][modem_num] = node[modem_key]

    config_lines = []
    config_lines.append("set debug stdout")
    config_lines.append("")
    config_lines.append("do impconfig.simh")
    config_lines.append(f"set imp num={imp_num}")
    config_lines.append("do impcode.simh")
    config_lines.append("")
    config_lines.append("")
    config_lines.append("# MODEM INTERFACES:")
    config_lines.append("")

    # Generate modem configurations
    for modem_num in [1, 2, 3]:
        modem_key = f'modem{modem_num}'
        if modem_key in imp_node:
            remote_imp = imp_node[modem_key]
            remote_imp_str = f"{remote_imp:02d}"

            # Find which modem on remote IMP connects back
            remote_modem = None
            if remote_imp in node_modems:
                for rm, target in node_modems[remote_imp].items():
                    if target == imp_num:
                        remote_modem = rm
                        break

            if remote_modem:
                # Calculate ports: 11[m][ii] format (5 digits, bidirectional)
                my_port = f"11{modem_num}{imp_num_str}"
                remote_port = f"11{remote_modem}{remote_imp_str}"

                config_lines.append(f"set mi{modem_num} enabled")
                config_lines.append(f"attach -u mi{modem_num} {my_port}::{remote_port}")
                config_lines.append("")

    config_lines.append("")
    config_lines.append("# HOST INTERFACES:")
    config_lines.append("")

    # Generate host configurations
    if imp_name in hosts_by_location:
        hosts = hosts_by_location[imp_name]

        # Build list of (host_index, hostname, front) and sort by host_index
        host_list = []
        for host_info in hosts:
            full_host_num = host_info['host']
            hostname = host_info.get('hostname', 'Unknown')
            front = host_info.get('front', 0)
            # Calculate host index: (host_number - imp_number) / 64
            host_index = int((full_host_num - imp_num) / 64)
            host_list.append((host_index, hostname, front))

        # Sort by host_index
        host_list.sort(key=lambda x: x[0])

        for host_index, hostname, front in host_list:
            # Calculate ports
            imp_tx = f"2{host_index}{imp_num_str}1"
            host_rx = f"2{host_index}{imp_num_str}2"

            # Comment out if host_index >= 2 OR front == 1
            if host_index < 2 and front != 1:
                # Active host interfaces (hi1, hi2 only, and front != 1)
                config_lines.append(f"set hi{host_index+1} enabled")
                config_lines.append(f"set hi{host_index+1} debug")
                config_lines.append(f"attach -u hi{host_index+1} {imp_tx}:localhost:{host_rx}")
                config_lines.append(f"# Host {host_index}: {hostname}")
                # Check if this IMP/host combination should have convert mode
                if (imp_num, host_index) in pdp_hosts:
                    config_lines.append(f"set hi{host_index+1} convert")
                config_lines.append("")
            else:
                # Commented out for future use (hi3+ or front=1)
                config_lines.append(f"#set hi{host_index+1} enabled")
                config_lines.append(f"#attach -u hi{host_index+1} {imp_tx}:localhost:{host_rx}")
                config_lines.append(f"# Host {host_index}: {hostname}")
                config_lines.append("")

    config_lines.append("go")
    config_lines.append("")

    # Write to file
    output_file = os.path.join(output_dir, f'imp{imp_num_str}.simh')
    with open(output_file, 'w') as f:
        f.write('\n'.join(config_lines))

    return output_file

def generate_network_config(nodes_data, node_names, hosts_by_location, output_dir='..'):
    """
    Generate a human-readable and machine-readable network configuration file
    This intermediate file documents the complete network topology before generating SIMH configs
    """
    from datetime import datetime

    # Build helper structures
    node_modems = {}
    for node in nodes_data:
        node_num = node['node']
        node_modems[node_num] = {}
        for modem_key in ['modem1', 'modem2', 'modem3']:
            if modem_key in node:
                modem_num = int(modem_key[-1])
                node_modems[node_num][modem_num] = node[modem_key]

    lines = []

    # File header
    lines.append("# ARPANET Network Configuration File")
    lines.append("# Generated from: arpanetNodes.js")
    lines.append(f"# Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("#")
    lines.append("# This file defines the complete ARPANET topology including:")
    lines.append("#   - IMP nodes and their modem connections")
    lines.append("#   - Host computers attached to each IMP")
    lines.append("#   - UDP port assignments for all interfaces")
    lines.append("#")
    lines.append("# Edit this file to modify the network topology, then use topol.py")
    lines.append("# to regenerate SIMH configuration files.")
    lines.append("#")
    lines.append("# Note: Text after '#' on each line is a comment for human reference only.")
    lines.append("#       Editing comment text will not affect the generated configs.")
    lines.append("#")
    lines.append("# IMPORTANT: Hosts are commented out in the following cases:")
    lines.append("#   1. host_index >= 2 (SIMH only supports hi1 and hi2)")
    lines.append("#   2. front=1 in arpanetNodes data (marked for future reference)")
    lines.append("")

    # ==============================================================================
    # SECTION 0: NETWORK OVERVIEW
    # ==============================================================================
    lines.append("# " + "=" * 78)
    lines.append("# SECTION 0: NETWORK OVERVIEW")
    lines.append("# " + "=" * 78)
    lines.append("")

    # Used IMPs
    used_imps = sorted([(node['node'], node['name1']) for node in nodes_data])
    used_imp_str = ", ".join([f"{num:02d}-{name}" for num, name in used_imps])
    lines.append("# Used IMPs:")
    lines.append(f"#{used_imp_str}")
    lines.append("")

    # Unused IMPs
    used_imp_numbers = set(node['node'] for node in nodes_data)
    all_imp_numbers = set(range(1, 64))
    unused_imp_numbers = sorted(all_imp_numbers - used_imp_numbers)
    if unused_imp_numbers:
        unused_imp_str = ", ".join([f"{num:02d}-unused" for num in unused_imp_numbers])
        lines.append("# Unused IMPs:")
        lines.append(f"#{unused_imp_str}")
        lines.append("")

    lines.append(f"# Total IMPs in network: {len(nodes_data)}")
    total_hosts = sum(len(hosts) for hosts in hosts_by_location.values())
    lines.append(f"# Total host computers: {total_hosts}")
    lines.append("")

    # ==============================================================================
    # SECTION 1: IMP NETWORK TOPOLOGY
    # ==============================================================================
    lines.append("# " + "=" * 78)
    lines.append("# SECTION 1: IMP NETWORK TOPOLOGY")
    lines.append("# " + "=" * 78)
    lines.append("#")
    lines.append("# Format: IMP <number> #<name>")
    lines.append("#         modem<n> -> <remote_imp> #<remote_name>")
    lines.append("")

    for node in nodes_data:
        imp_num = node['node']
        imp_name = node['name1']

        lines.append(f"IMP {imp_num:02d} #{imp_name}")

        for modem_num in [1, 2, 3]:
            modem_key = f'modem{modem_num}'
            if modem_key in node:
                remote_imp = node[modem_key]
                remote_name = node_names.get(remote_imp, "Unknown")
                lines.append(f"  modem{modem_num} -> {remote_imp:02d} #{remote_name}")

        lines.append("")

    # ==============================================================================
    # SECTION 2: HOST ATTACHMENTS
    # ==============================================================================
    lines.append("# " + "=" * 78)
    lines.append("# SECTION 2: HOST ATTACHMENTS")
    lines.append("# " + "=" * 78)
    lines.append("#")
    lines.append("# Format: IMP <number> #<name>")
    lines.append("#         host<n> <calculated_index> <hostname> #<full_arpanet_host_number>")
    lines.append("#")
    lines.append("# Note: calculated_index = (full_arpanet_host_number - imp_number) / 64")
    lines.append("#       host0 attaches to hi1, host1 to hi2, host2 to hi3, etc.")
    lines.append("#")
    lines.append("# IMPORTANT: Only host0 and host1 are active (SIMH supports hi1/hi2 only).")
    lines.append("#            Hosts 2+ are commented out for future reference.")
    lines.append("")

    for node in nodes_data:
        imp_num = node['node']
        imp_name = node['name1']

        if imp_name in hosts_by_location:
            hosts = hosts_by_location[imp_name]
            if hosts:
                lines.append(f"IMP {imp_num:02d} #{imp_name}")

                # Build list of (host_index, full_host_num, hostname, front) and sort by host_index
                host_list = []
                for host_info in hosts:
                    full_host_num = host_info['host']
                    hostname = host_info['hostname']
                    front = host_info.get('front', 0)
                    # Calculate host index: (host_number - imp_number) / 64
                    host_index = int((full_host_num - imp_num) / 64)
                    host_list.append((host_index, full_host_num, hostname, front))

                # Sort by host_index (0, 1, 2, 3...)
                host_list.sort(key=lambda x: x[0])

                # Generate lines, commenting out if host_index >= 2 OR front == 1
                for host_index, full_host_num, hostname, front in host_list:
                    line = f"  host{host_index} {host_index} {hostname} #{full_host_num}"
                    if host_index >= 2 or front == 1:
                        line = "#" + line
                    lines.append(line)

                lines.append("")

    # ==============================================================================
    # SECTION 3: PORT ASSIGNMENTS
    # ==============================================================================
    lines.append("# " + "=" * 78)
    lines.append("# SECTION 3: PORT ASSIGNMENTS")
    lines.append("# " + "=" * 78)
    lines.append("#")
    lines.append("# Format: IMP <number> #<name>")
    lines.append("#         mi<n> <local_port> <remote_port> -> IMP <remote_imp> #<remote_name>")
    lines.append("#         hi<n> <imp_tx> <host_rx> host<n> <hostname>")
    lines.append("#")
    lines.append("# Port numbering scheme:")
    lines.append("#   Modem ports: 11[m][ii] where m=modem#, ii=IMP# (bidirectional, 5 digits)")
    lines.append("#   Host ports:  2[h][ii][d] where h=host#, ii=IMP#, d=1(TX)/2(RX)")
    lines.append("#")
    lines.append("# IMPORTANT: Only hi1 and hi2 are active (SIMH limitation).")
    lines.append("#            hi3+ are commented out for future reference.")
    lines.append("")

    for node in nodes_data:
        imp_num = node['node']
        imp_name = node['name1']
        imp_num_str = f"{imp_num:02d}"

        lines.append(f"IMP {imp_num_str} #{imp_name}")

        # Modem interfaces
        has_modems = False
        for modem_num in [1, 2, 3]:
            modem_key = f'modem{modem_num}'
            if modem_key in node:
                has_modems = True
                remote_imp = node[modem_key]
                remote_imp_str = f"{remote_imp:02d}"
                remote_name = node_names.get(remote_imp, "Unknown")

                # Find which modem on remote IMP connects back
                remote_modem = None
                if remote_imp in node_modems:
                    for rm, target in node_modems[remote_imp].items():
                        if target == imp_num:
                            remote_modem = rm
                            break

                if remote_modem:
                    # Calculate ports: 11[m][ii] format (5 digits, bidirectional)
                    my_port = f"11{modem_num}{imp_num_str}"
                    remote_port = f"11{remote_modem}{remote_imp_str}"

                    lines.append(f"  mi{modem_num} {my_port} {remote_port} -> IMP {remote_imp_str} #{remote_name}")

        # Host interfaces
        if imp_name in hosts_by_location:
            hosts = hosts_by_location[imp_name]

            # Build list of (host_index, imp_tx, host_rx, hostname, front) and sort by host_index
            host_if_list = []
            for host_info in hosts:
                full_host_num = host_info['host']
                hostname = host_info['hostname']
                front = host_info.get('front', 0)
                host_index = int((full_host_num - imp_num) / 64)

                # Calculate ports
                imp_tx = f"2{host_index}{imp_num_str}1"
                host_rx = f"2{host_index}{imp_num_str}2"

                host_if_list.append((host_index, imp_tx, host_rx, hostname, front))

            # Sort by host_index (0, 1, 2, 3...)
            host_if_list.sort(key=lambda x: x[0])

            # Generate lines, commenting out if host_index >= 2 OR front == 1
            for host_index, imp_tx, host_rx, hostname, front in host_if_list:
                line = f"  hi{host_index+1} {imp_tx} {host_rx} host{host_index} {hostname}"
                if host_index >= 2 or front == 1:
                    line = "#" + line
                lines.append(line)

        lines.append("")

    # ==============================================================================
    # SECTION 4: NODE COORDINATES
    # ==============================================================================
    lines.append("# " + "=" * 78)
    lines.append("# SECTION 4: NODE COORDINATES")
    lines.append("# " + "=" * 78)
    lines.append("#")
    lines.append("# Format: IMP <number> <x> <y> #<name>")
    lines.append("#")
    lines.append("# Coordinates normalized to 60x20 grid (width x height)")
    lines.append("# Origin is at bottom-left (0,0), top-right is (60,20)")
    lines.append("")

    # Collect all nodes that have x,y coordinates
    nodes_with_coords = [node for node in nodes_data if 'x' in node and 'y' in node]

    if nodes_with_coords:
        # Find min/max bounds for normalization
        x_vals = [node['x'] for node in nodes_with_coords]
        y_vals = [node['y'] for node in nodes_with_coords]

        x_min, x_max = min(x_vals), max(x_vals)
        y_min, y_max = min(y_vals), max(y_vals)

        # Generate coordinate entries, sorted by IMP number
        coord_entries = []
        for node in nodes_with_coords:
            imp_num = node['node']
            imp_name = node['name1']
            x_orig = node['x']
            y_orig = node['y']

            # Normalize to 60x20 grid
            if x_max != x_min:
                x_norm = ((x_orig - x_min) / (x_max - x_min)) * 60.0
            else:
                x_norm = 30.0  # Center if all x values are the same

            if y_max != y_min:
                # Invert y-axis: screen coordinates are top-down, we want bottom-up
                y_norm = (1.0 - (y_orig - y_min) / (y_max - y_min)) * 20.0
            else:
                y_norm = 10.0  # Center if all y values are the same

            coord_entries.append((imp_num, x_norm, y_norm, imp_name))

        # Sort by IMP number
        coord_entries.sort(key=lambda x: x[0])

        # Output coordinate lines
        for imp_num, x_norm, y_norm, imp_name in coord_entries:
            lines.append(f"IMP {imp_num:02d} {x_norm:5.1f} {y_norm:5.1f} #{imp_name}")

        lines.append("")
    else:
        lines.append("# No coordinate data available")
        lines.append("")

    # Write to file
    output_file = os.path.join(output_dir, 'arpanet-topology.conf')
    with open(output_file, 'w') as f:
        f.write('\n'.join(lines))

    return output_file

def generate_start_script(nodes_data, node_names, hosts_by_location, pdp_hosts, output_dir='..'):
    """
    Generate a unified bash script to start/stop all IMPs and NCP daemons in screen sessions
    Uses embedded bash arrays and loops for elegance
    Excludes ncpd entries for IMP/host combinations that have PDP-10s (listed in pdp_hosts)
    """
    from datetime import datetime

    lines = []

    # Script header
    lines.append("#!/bin/bash")
    lines.append("#")
    lines.append("# ARPANET Network Control Script")
    lines.append("# Generated from: arpanetNodes.js")
    lines.append(f"# Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append("#")
    lines.append("# Usage:")
    lines.append("#   ./arpanet start   - Start all IMPs and NCP daemons")
    lines.append("#   ./arpanet stop    - Stop all IMPs and NCP daemons")
    lines.append("#   ./arpanet         - Show this help")
    lines.append("#")
    lines.append("")

    # Build IMP array
    lines.append("# ============================================================")
    lines.append("# IMP LIST (impnum:impname)")
    lines.append("# ============================================================")
    lines.append("declare -a IMPS=(")

    imp_count = 0
    for node in sorted(nodes_data, key=lambda x: x['node']):
        imp_num = node['node']
        imp_name = node['name1']
        imp_num_str = f"{imp_num:02d}"
        lines.append(f'  "{imp_num_str}:{imp_name}"')
        imp_count += 1

    lines.append(")")
    lines.append("")

    # Build NCP array
    lines.append("# ============================================================")
    lines.append("# NCP LIST (imp:host_idx:full_host:tx_port:rx_port:hostname)")
    lines.append("# ============================================================")
    lines.append("# Note: Lines starting with '#' are commented out because")
    lines.append("#       they have PDP-10s (listed in pdp-hosts file)")
    lines.append("declare -a NCPS=(")

    ncp_count = 0
    for node in sorted(nodes_data, key=lambda x: x['node']):
        imp_num = node['node']
        imp_name = node['name1']
        imp_num_str = f"{imp_num:02d}"

        if imp_name in hosts_by_location:
            hosts = hosts_by_location[imp_name]

            # Build list of active hosts (host_index < 2 and front != 1)
            active_hosts = []
            for host_info in hosts:
                full_host_num = host_info['host']
                hostname = host_info.get('hostname', 'Unknown')
                front = host_info.get('front', 0)
                host_index = int((full_host_num - imp_num) / 64)

                # Only include active hosts (hi1, hi2 only, and not front=1)
                if host_index < 2 and front != 1:
                    active_hosts.append((host_index, full_host_num, hostname))

            # Sort by host_index
            active_hosts.sort(key=lambda x: x[0])

            for host_index, full_host_num, hostname in active_hosts:
                # Calculate ports for this host interface
                port_tx = f"2{host_index}{imp_num_str}1"
                port_rx = f"2{host_index}{imp_num_str}2"

                # Format full_host_num with zero-padding (at least 2 digits)
                full_host_str = f"{full_host_num:02d}"

                # Check if this IMP/host has a PDP-10 (should not run ncpd)
                if (imp_num, host_index) in pdp_hosts:
                    # Comment out - this port is for a PDP-10, not ncpd
                    lines.append(f'  #"{imp_num_str}:{host_index}:{full_host_str}:{port_tx}:{port_rx}:{hostname}"  # PDP-10 host')
                else:
                    lines.append(f'  "{imp_num_str}:{host_index}:{full_host_str}:{port_tx}:{port_rx}:{hostname}"')
                    ncp_count += 1

    lines.append(")")
    lines.append("")

    # Function: show_usage
    lines.append("# ============================================================")
    lines.append("# FUNCTION: show_usage")
    lines.append("# ============================================================")
    lines.append("show_usage() {")
    lines.append('  echo "ARPANET Network Control Script"')
    lines.append('  echo ""')
    lines.append('  echo "Usage: $0 {start|stop|start-imps|start-ncpds|stop-imps|stop-ncpds}"')
    lines.append('  echo ""')
    lines.append('  echo "Commands:"')
    lines.append('  echo "  start        Start all IMPs, wait, then start all NCPs"')
    lines.append('  echo "  stop         Stop all NCPs, then stop all IMPs"')
    lines.append('  echo "  start-imps   Start only IMP simulators (NCPs will reattach)"')
    lines.append('  echo "  start-ncpds  Start only NCP daemons (IMPs will accept connections)"')
    lines.append('  echo "  stop-imps    Stop only IMP simulators"')
    lines.append('  echo "  stop-ncpds   Stop only NCP daemons and clean up sockets"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: start_imps
    lines.append("# ============================================================")
    lines.append("# FUNCTION: start_imps")
    lines.append("# ============================================================")
    lines.append("start_imps() {")
    lines.append('  echo "Starting IMP simulators..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Create logfiles directory if it doesn't exist")
    lines.append("  mkdir -p ./logfiles")
    lines.append("")
    lines.append("  # Start IMP simulators")
    lines.append("  for entry in \"${IMPS[@]}\"; do")
    lines.append("    IFS=: read -r impnum impname <<< \"$entry\"")
    lines.append('    echo "  Starting IMP $impnum: $impname"')
    lines.append("    screen -dmS imp$impnum ./h316 ./imp$impnum.simh >./logfiles/imp$impnum.log 2>&1")
    lines.append("  done")
    lines.append("")
    lines.append(f'  echo "Started {imp_count} IMP simulators"')
    lines.append('  echo ""')
    lines.append('  echo "Use \\"screen -ls\\" to list all sessions"')
    lines.append('  echo "Use \\"screen -r imp01\\" to attach to IMP 01"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: start_ncpds
    lines.append("# ============================================================")
    lines.append("# FUNCTION: start_ncpds")
    lines.append("# ============================================================")
    lines.append("start_ncpds() {")
    lines.append('  echo "Starting NCP daemons..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Create logfiles directory if it doesn't exist")
    lines.append("  mkdir -p ./logfiles")
    lines.append("")
    lines.append("  # Start NCP daemons")
    lines.append("  for entry in \"${NCPS[@]}\"; do")
    lines.append("    IFS=: read -r imp hostidx fullhost porttx portrx hostname <<< \"$entry\"")
    lines.append('    echo "  Starting NCP for IMP $imp host $hostidx: $hostname (ARPANET #$fullhost)"')
    lines.append('    export NCP="$PWD/ncp$fullhost"')
    lines.append('    rm -f "$NCP"')
    lines.append('    screen -dmS ncp$fullhost ./ncpd localhost $porttx $portrx 2>./logfiles/ncp$fullhost.log')
    lines.append("  done")
    lines.append("")
    lines.append(f'  echo "Started {ncp_count} NCP daemons"')
    lines.append('  echo ""')
    lines.append('  echo "Use \\"screen -ls\\" to list all sessions"')
    lines.append('  echo "Use \\"screen -r ncp2\\" to attach to NCP daemon for host 2"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: start_network
    lines.append("# ============================================================")
    lines.append("# FUNCTION: start_network")
    lines.append("# ============================================================")
    lines.append("start_network() {")
    lines.append('  echo "Starting ARPANET network..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Start IMPs first")
    lines.append("  start_imps")
    lines.append("")
    lines.append("  # Wait for IMPs to initialize")
    lines.append('  echo "Waiting for IMPs to initialize..."')
    lines.append("  sleep 3")
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Start NCPs")
    lines.append("  start_ncpds")
    lines.append("")
    lines.append('  echo "ARPANET network started successfully!"')
    lines.append(f'  echo "  IMPs running: {imp_count}"')
    lines.append(f'  echo "  NCP daemons: {ncp_count}"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: stop_imps
    lines.append("# ============================================================")
    lines.append("# FUNCTION: stop_imps")
    lines.append("# ============================================================")
    lines.append("stop_imps() {")
    lines.append('  echo "Stopping IMP simulators..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Stop all IMP processes")
    lines.append('  echo "Stopping h316 processes..."')
    lines.append("  pkill -9 h316 2>/dev/null")
    lines.append("  sleep 1")
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Show processes after cleanup")
    lines.append('  echo "h316 processes after cleanup:"')
    lines.append("  ps aux | grep '[h]316' || echo '  (none)'")
    lines.append('  echo ""')
    lines.append("")
    lines.append(f'  echo "Stopped {imp_count} IMP simulators"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: stop_ncpds
    lines.append("# ============================================================")
    lines.append("# FUNCTION: stop_ncpds")
    lines.append("# ============================================================")
    lines.append("stop_ncpds() {")
    lines.append('  echo "Stopping NCP daemons..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Stop all NCP processes")
    lines.append('  echo "Stopping ncpd processes..."')
    lines.append("  pkill -9 ncpd 2>/dev/null")
    lines.append("  sleep 1")
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Show processes after cleanup")
    lines.append('  echo "ncpd processes after cleanup:"')
    lines.append("  ps aux | grep '[n]cpd' || echo '  (none)'")
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Clean up all NCP socket files (only sockets matching ncp[0-9]*)")
    lines.append('  echo "Cleaning up NCP socket files..."')
    lines.append('  for f in ncp[0-9]*; do')
    lines.append('    [ -S "$f" ] && rm -f "$f"')
    lines.append('  done')
    lines.append('  echo ""')
    lines.append("")
    lines.append(f'  echo "Stopped {ncp_count} NCP daemons"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Function: stop_network
    lines.append("# ============================================================")
    lines.append("# FUNCTION: stop_network")
    lines.append("# ============================================================")
    lines.append("stop_network() {")
    lines.append('  echo "Stopping ARPANET network..."')
    lines.append('  echo ""')
    lines.append("")
    lines.append("  # Stop NCPs first (cleaner disconnect)")
    lines.append("  stop_ncpds")
    lines.append("")
    lines.append("  # Brief pause")
    lines.append("  sleep 1")
    lines.append("")
    lines.append("  # Stop IMPs")
    lines.append("  stop_imps")
    lines.append("")
    lines.append('  echo "ARPANET network stopped successfully!"')
    lines.append('  echo ""')
    lines.append("}")
    lines.append("")

    # Main script logic
    lines.append("# ============================================================")
    lines.append("# MAIN")
    lines.append("# ============================================================")
    lines.append("")
    lines.append("case \"$1\" in")
    lines.append("  start)")
    lines.append("    start_network")
    lines.append("    ;;")
    lines.append("  stop)")
    lines.append("    stop_network")
    lines.append("    ;;")
    lines.append("  start-imps)")
    lines.append("    start_imps")
    lines.append("    ;;")
    lines.append("  start-ncpds)")
    lines.append("    start_ncpds")
    lines.append("    ;;")
    lines.append("  stop-imps)")
    lines.append("    stop_imps")
    lines.append("    ;;")
    lines.append("  stop-ncpds)")
    lines.append("    stop_ncpds")
    lines.append("    ;;")
    lines.append("  *)")
    lines.append("    show_usage")
    lines.append("    exit 1")
    lines.append("    ;;")
    lines.append("esac")

    # Write to file
    output_file = os.path.join(output_dir, 'arpanet')
    with open(output_file, 'w') as f:
        f.write('\n'.join(lines))
        f.write('\n')

    # Make executable
    os.chmod(output_file, 0o755)

    return output_file

def parse_topology_config(config_file):
    """
    Parse arpanet-topology.conf to extract network topology
    Returns: (nodes_data, node_names, hosts_by_location)
    """
    if not os.path.exists(config_file):
        raise FileNotFoundError(f"Topology config file not found: {config_file}")

    with open(config_file, 'r') as f:
        content = f.read()

    nodes_data = []
    node_names = {}
    hosts_by_location = {}

    # Parse Section 1: IMP Network Topology
    section1_match = re.search(r'# SECTION 1: IMP NETWORK TOPOLOGY.*?# SECTION 2:', content, re.DOTALL)
    if section1_match:
        section1 = section1_match.group(0)

        current_imp = None
        for line in section1.split('\n'):
            # Match: IMP 01 #UCLA
            imp_match = re.match(r'^IMP (\d+) #(.+)$', line.strip())
            if imp_match:
                imp_num = int(imp_match.group(1))
                imp_name = imp_match.group(2).strip()

                current_imp = {
                    'node': imp_num,
                    'name1': imp_name
                }
                nodes_data.append(current_imp)
                node_names[imp_num] = imp_name

            # Match: modem1 -> 02 #SRI
            elif current_imp and line.strip().startswith('modem'):
                modem_match = re.match(r'^\s*modem(\d+) -> (\d+)', line)
                if modem_match:
                    modem_num = int(modem_match.group(1))
                    remote_imp = int(modem_match.group(2))
                    current_imp[f'modem{modem_num}'] = remote_imp

    # Parse Section 2: Host Attachments
    section2_match = re.search(r'# SECTION 2: HOST ATTACHMENTS.*?# SECTION 3:', content, re.DOTALL)
    if section2_match:
        section2 = section2_match.group(0)

        current_imp_name = None
        for line in section2.split('\n'):
            # Match: IMP 01 #UCLA
            imp_match = re.match(r'^IMP (\d+) #(.+)$', line.strip())
            if imp_match:
                current_imp_name = imp_match.group(2).strip()
                if current_imp_name not in hosts_by_location:
                    hosts_by_location[current_imp_name] = []

            # Match: host0 0 UCLA-NMC #1  or  #  host2 2 MIT-AI #134 (commented)
            elif current_imp_name:
                # Skip commented lines for host parsing (we only want active hosts)
                if line.strip().startswith('#'):
                    continue

                host_match = re.match(r'^\s*host(\d+) (\d+) (.+?) #(\d+)$', line)
                if host_match:
                    host_idx = int(host_match.group(1))
                    hostname = host_match.group(3).strip()
                    full_host_num = int(host_match.group(4))

                    hosts_by_location[current_imp_name].append({
                        'host': full_host_num,
                        'hostname': hostname,
                        'front': 0  # Active hosts don't have front=1
                    })

    return nodes_data, node_names, hosts_by_location

def verify_topology_config(config_file):
    """
    Verify arpanet-topology.conf for consistency
    Returns list of errors found
    """
    errors = []

    try:
        nodes_data, node_names, hosts_by_location = parse_topology_config(config_file)
    except Exception as e:
        errors.append(f"Failed to parse topology config: {str(e)}")
        return errors

    # Validate connections are bidirectional
    connection_errors = validate_connections(nodes_data)
    errors.extend(connection_errors)

    # Verify each IMP has valid data
    for node in nodes_data:
        if 'node' not in node:
            errors.append(f"IMP missing node number")
        if 'name1' not in node:
            errors.append(f"IMP {node.get('node', '?')} missing name")

    return errors

def create_backup(config_dir='..'):
    """
    Create a timestamped backup of all .simh files and arpanet-topology.conf
    Returns the path to the backup file
    """
    from datetime import datetime

    # Generate timestamp
    timestamp = datetime.now().strftime('%Y%m%d-%H%M%S')
    backup_filename = f'arpanet-topology-backup-{timestamp}.zip'
    backup_path = os.path.join(config_dir, backup_filename)

    # Find all files to backup
    files_to_backup = []

    # Add all .simh files
    simh_files = glob.glob(os.path.join(config_dir, '*.simh'))
    files_to_backup.extend(simh_files)

    # Add arpanet-topology.conf if it exists
    topology_conf = os.path.join(config_dir, 'arpanet-topology.conf')
    if os.path.exists(topology_conf):
        files_to_backup.append(topology_conf)

    # Add arpanet control script if it exists (new name)
    control_script = os.path.join(config_dir, 'arpanet')
    if os.path.exists(control_script):
        files_to_backup.append(control_script)

    # Also add old arpanet-start if it exists (backward compatibility)
    old_start_script = os.path.join(config_dir, 'arpanet-start')
    if os.path.exists(old_start_script):
        files_to_backup.append(old_start_script)

    if not files_to_backup:
        print("   No existing files to backup")
        return None

    # Create zip file
    with zipfile.ZipFile(backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file_path in files_to_backup:
            # Add file to zip with just the basename (no directory path)
            arcname = os.path.basename(file_path)
            zipf.write(file_path, arcname)

    return backup_path

def ensure_gitignore_entry(gitignore_path, pattern):
    """
    Ensure a pattern is in .gitignore, add it if not present
    """
    # Read existing .gitignore if it exists
    existing_patterns = set()
    if os.path.exists(gitignore_path):
        with open(gitignore_path, 'r') as f:
            existing_patterns = set(line.strip() for line in f if line.strip() and not line.startswith('#'))

    # Check if pattern already exists
    if pattern in existing_patterns:
        return False  # Already present

    # Add pattern
    with open(gitignore_path, 'a') as f:
        if existing_patterns:  # If file exists and not empty, add newline first
            f.write('\n')
        f.write(f'# ARPANET topology backups\n')
        f.write(f'{pattern}\n')

    return True  # Added new pattern

def main():
    """
    Main function to load data and visualize the ARPANET network
    """
    # Path to the JavaScript file (relative to this script)
    js_file = '../../arpa/assets/js/arpanet-nodes.js'
    config_output_dir = '..'

    print("ARPANET Network Topology Visualizer & Config Generator")
    print("\na.k.a. Edward the topologist")
    print("\nI will use two input files. \nThe arpanet topology itself. This can")
    print("come either from the .js original, or from a previously generated .conf")
    print("Then, the pdp-hosts file, which tells me where to expect simh PDP-10s\n")
    print("=" * 60)
    print("\nPlease select an option:")
    print("  1) Generate SIMH config files from arpanetNodes.js")
    print("  2) Generate SIMH config files from arpanet-topology.conf")
    print("  3) Verify existing SIMH config files")
    print()

    while True:
        choice = input("Enter your choice (1, 2, or 3): ").strip()
        if choice in ['1', '2', '3']:
            break
        print("Invalid choice. Please enter 1, 2, or 3.")

    print()

    # Parse data based on choice
    if choice in ['1', '3']:
        # Options 1 and 3: Parse from JavaScript file
        print("1. Parsing data from:", js_file)
        nodes = parse_arpanet_nodes(js_file)
        print(f"   Found {len(nodes)} IMP nodes")

        # Parse host computers
        hosts_by_location = parse_host_computers(js_file)
        total_hosts = sum(len(hosts) for hosts in hosts_by_location.values())
        print(f"   Found {total_hosts} host computers across {len(hosts_by_location)} locations")

        # Build node_names mapping
        node_names = {node['node']: node['name1'] for node in nodes}

    elif choice == '2':
        # Option 2: Parse from arpanet-topology.conf
        topology_conf = os.path.join(config_output_dir, 'arpanet-topology.conf')
        print("1. Parsing data from:", topology_conf)

        try:
            nodes, node_names, hosts_by_location = parse_topology_config(topology_conf)
            print(f"   Found {len(nodes)} IMP nodes")
            total_hosts = sum(len(hosts) for hosts in hosts_by_location.values())
            print(f"   Found {total_hosts} host computers")
        except FileNotFoundError:
            print("\n" + "=" * 60)
            print("ERROR: arpanet-topology.conf not found!")
            print("Please run option 1 first to generate the topology file.")
            print("=" * 60)
            return
        except Exception as e:
            print("\n" + "=" * 60)
            print(f"ERROR: Failed to parse topology config: {str(e)}")
            print("=" * 60)
            return

    # Validate connections (for all options)
    print("\n2. Validating modem connections...")
    errors = validate_connections(nodes)
    if errors:
        print(f"   ❌ Found {len(errors)} connection error(s):")
        for error in errors:
            print(error)
        print("\n" + "=" * 60)
        print("ERROR: Data validation failed!")
        print("All modem connections must be bidirectional.")
        print("Please fix the data errors before proceeding.")
        print("=" * 60)
        return  # Exit without proceeding
    else:
        print("   ✓ Modem connection ports between IMPs match")

    if choice == '1':
        # OPTION 1: Generate new config files

        # Build node_names mapping (needed for config generation)
        node_names = {node['node']: node['name1'] for node in nodes}

        # Create backup of existing files
        print("\n3. Creating backup of existing configuration...")
        backup_path = create_backup(config_output_dir)
        if backup_path:
            print(f"   ✓ Backup created: {os.path.basename(backup_path)}")

            # Update .gitignore to include backup files
            gitignore_path = os.path.join(config_output_dir, '.gitignore')
            if ensure_gitignore_entry(gitignore_path, 'arpanet-topology-backup-*.zip'):
                print(f"   ✓ Added backup pattern to .gitignore")
        else:
            print("   ⚠ No existing files to backup (first run)")

        # Generate network configuration file
        print("\n4. Generating network configuration file...")
        config_file = generate_network_config(nodes, node_names, hosts_by_location, config_output_dir)
        print(f"   ✓ Created {config_file}")

        # Parse pdp-hosts file
        pdp_hosts = parse_pdp_hosts_file()

        # Generate control script
        print("\n5. Generating control script (arpanet)...")
        control_script = generate_start_script(nodes, node_names, hosts_by_location, pdp_hosts, config_output_dir)
        print(f"   ✓ Created {control_script}")

        print("\n6. Generating SIMH configuration files...")

        # Delete existing imp[0-9][0-9].simh files (but NOT impcode.simh or impconfig.simh)
        existing_configs = glob.glob(os.path.join(config_output_dir, 'imp[0-9][0-9].simh'))
        if existing_configs:
            print(f"   Deleting {len(existing_configs)} existing config files...")
            for config_file in existing_configs:
                os.remove(config_file)

        # Generate new config files
        generated_files = []
        for node in nodes:
            output_file = generate_imp_config(node, nodes, node_names, hosts_by_location, pdp_hosts, config_output_dir)
            generated_files.append(output_file)

        print(f"   ✓ Generated {len(generated_files)} IMP configuration files in {config_output_dir}/")

        # Verify the generated configs
        print("\n7. Verifying generated config files...")
        verify_errors = verify_simh_configs(nodes, config_output_dir)
        if verify_errors:
            print(f"   ⚠ Found {len(verify_errors)} verification error(s):")
            for error in verify_errors:
                print(f"     {error}")
        else:
            print("   ✓ All generated config files verified successfully")

    elif choice == '2':
        # OPTION 2: Generate SIMH configs from arpanet-topology.conf
        # NOTE: We do NOT regenerate arpanet-topology.conf itself

        # Create backup of existing files (including arpanet-topology.conf)
        print("\n3. Creating backup of existing configuration...")
        backup_path = create_backup(config_output_dir)
        if backup_path:
            print(f"   ✓ Backup created: {os.path.basename(backup_path)}")

            # Update .gitignore to include backup files
            gitignore_path = os.path.join(config_output_dir, '.gitignore')
            if ensure_gitignore_entry(gitignore_path, 'arpanet-topology-backup-*.zip'):
                print(f"   ✓ Added backup pattern to .gitignore")
        else:
            print("   ⚠ No existing files to backup")

        # Parse pdp-hosts file
        pdp_hosts = parse_pdp_hosts_file()

        # Generate control script (from parsed topology)
        print("\n4. Generating control script (arpanet)...")
        control_script = generate_start_script(nodes, node_names, hosts_by_location, pdp_hosts, config_output_dir)
        print(f"   ✓ Created {control_script}")

        print("\n5. Generating SIMH configuration files...")

        # Delete existing imp[0-9][0-9].simh files (but NOT impcode.simh or impconfig.simh)
        existing_configs = glob.glob(os.path.join(config_output_dir, 'imp[0-9][0-9].simh'))
        if existing_configs:
            print(f"   Deleting {len(existing_configs)} existing config files...")
            for config_file in existing_configs:
                os.remove(config_file)

        # Generate new config files
        generated_files = []
        for node in nodes:
            output_file = generate_imp_config(node, nodes, node_names, hosts_by_location, pdp_hosts, config_output_dir)
            generated_files.append(output_file)

        print(f"   ✓ Generated {len(generated_files)} IMP configuration files in {config_output_dir}/")

        # Verify the generated configs
        print("\n6. Verifying generated config files...")
        verify_errors = verify_simh_configs(nodes, config_output_dir)
        if verify_errors:
            print(f"   ⚠ Found {len(verify_errors)} verification error(s):")
            for error in verify_errors:
                print(f"     {error}")
        else:
            print("   ✓ All generated config files verified successfully")

    elif choice == '3':
        # OPTION 3: Verify existing config files only
        print("\n3. Verifying existing SIMH config files...")
        verify_errors = verify_simh_configs(nodes, config_output_dir)

        if verify_errors:
            print(f"\n   ❌ Found {len(verify_errors)} verification error(s):")
            for error in verify_errors:
                print(f"     {error}")
            print("\n" + "=" * 60)
            print("Config files have errors. Please review and fix.")
            print("=" * 60)
        else:
            print("\n   ✓ All config files verified successfully!")
            print("   ✓ Network connections are intact")

        # Skip visualization for verify-only mode
        print("\nVerification complete. Exiting without visualization.")
        return

    # Continue with network analysis and visualization (for options 1 and 2)
    # Check for unused IMP numbers
    used_imp_numbers = set(node['node'] for node in nodes)
    all_imp_numbers = set(range(1, 64))
    unused_imp_numbers = sorted(all_imp_numbers - used_imp_numbers)

    if unused_imp_numbers:
        print(f"\n   Unused IMP numbers (1-63): {', '.join(map(str, unused_imp_numbers))}")
    else:
        print("\n   All IMP numbers (1-63) are in use")

    # Build the network graph
    print("\n8. Building network graph...")
    G, node_names = build_network_graph(nodes)
    print(f"   Graph has {G.number_of_nodes()} nodes and {G.number_of_edges()} edges")

    # Show network statistics
    print("\n9. Network Statistics:")
    print(f"   - Average degree: {sum(dict(G.degree()).values()) / G.number_of_nodes():.2f}")
    print(f"   - Network density: {nx.density(G):.3f}")
    print(f"   - Is connected: {nx.is_connected(G)}")
    if not nx.is_connected(G):
        components = list(nx.connected_components(G))
        print(f"   - Number of components: {len(components)}")
        print(f"   - Largest component size: {len(max(components, key=len))}")

    # Create layout with fixed positions
    print("\n10. Creating network layout...")
    pos = create_fixed_layout(G, node_names, nodes)

    # Visualize
    print("\n11. Displaying network visualization...")
    visualize_network(G, pos, node_names, hosts_by_location)

if __name__ == '__main__':
    main()
