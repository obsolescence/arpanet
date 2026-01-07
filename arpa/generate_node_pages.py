#!/usr/bin/env python3
"""
Generate ARPANET node detail pages from template and arpanet-nodes.json
"""

import json
import re
from pathlib import Path

# Read the arpanet-nodes.json file
json_file = Path(__file__).parent / 'arpanet-nodes.json'

try:
    with open(json_file, 'r') as f:
        nodes = json.load(f)
except FileNotFoundError:
    print(f"Error: Could not find {json_file}")
    print("Please ensure arpanet-nodes.json exists in the same directory as this script.")
    exit(1)
except json.JSONDecodeError as e:
    print(f"Error parsing JSON file: {e}")
    exit(1)

# Nodes are now already structured by IMP, just sort by IMP number
sorted_imps = sorted([node['imp_number'] for node in nodes])

print(f"Found {len(sorted_imps)} unique ARPANET nodes:")
print()

# Read template
template_file = Path(__file__).parent / 'arpanet-node-x-template.html'
with open(template_file, 'r') as f:
    template_content = f.read()

# Extract the host section template
host_section_match = re.search(
    r'<!-- BEGIN_HOST_SECTION -->(.+?)<!-- END_HOST_SECTION -->',
    template_content,
    re.DOTALL
)
if not host_section_match:
    print("Error: Could not find host section markers in template")
    exit(1)

host_section_template = host_section_match.group(1)

# Generate files for each node
for node in nodes:
    imp_num = node['imp_number']
    name1 = node['name']

    # Get all hosts for this node
    hosts = node.get('hosts', [])

    # Find computers with hostnames (excluding pure IMP entries)
    computers = {}
    for host in hosts:
        if host.get('hostname'):  # Has hostname = it's a computer
            host_num = host.get('host_number', 0)
            computers[host_num] = host

    # Generate navigation links based on actual hosts
    nav_links = f'<a href="#c1">Node {imp_num}: {name1}</a>'
    for host_num in sorted(computers.keys()):
        host = computers[host_num]
        host_name = host.get('name2', f'Computer {host_num}')
        hostname = host.get('hostname', '')
        nav_links += f' &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;| &nbsp;&nbsp;&nbsp;&nbsp;<a href="#c{host_num}">Host {imp_num},{host_num}: {host_name}, \'{hostname}\'</a>'

    # Add PeopleAnecdotes link to navigation if the section exists
    if node.get('people_anecdotes'):
        nav_links += f' &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;| &nbsp;&nbsp;&nbsp;&nbsp;<a href="#canecdotes">People & Anecdotes</a>'

    # Prepare base replacements (same for all hosts in this node)
    # Support both old 'link_list' and new 'link_list_node' for backward compatibility
    node_links = node.get('link_list_node') or node.get('link_list') or f'<a href="#">{name1} Home</a><br>'

    base_replacements = {
        '{{{1}}}': str(imp_num),
        '{{{UCLA}}}': name1,
        '{{{NAVIGATION_LINKS}}}': nav_links,
        '{{{Introduction text}}}': node.get('introduction', f"Node {imp_num}: {name1} was a major node on the ARPANET."),
        '{{{Introduction2 text}}}': node.get('introduction2', f"Additional information about {name1}."),
        '{{{PeopleAnecdotes}}}': node.get('people_anecdotes', f"Stories and anecdotes about {name1}."),
        '{{{link-list}}}': node_links,
        '{{{link_list_node}}}': node_links,
        '{{{image-list}}}': node.get('image_list', f'<a href="images/arpa/{imp_num}/" class="image"><img src="images/placeholder.png" alt="Node images" /></a>'),
    }

    # Generate host sections for each computer
    host_sections = []
    for host_num in sorted(computers.keys()):
        host = computers[host_num]

        # Create a copy of the host template for this specific host
        host_html = host_section_template

        # Replace HOST_INDEX and HOST_NAME placeholders
        # Support host-specific link lists
        host_link_key = f'link_list_host_{host_num}'
        host_links = node.get(host_link_key, '')

        host_replacements = {
            '{{{HOST_INDEX}}}': str(host_num),
            '{{{HOST_NAME}}}': host.get('name2', f'Computer {host_num}'),
            '{{{1,HOST_INDEX}}}': f"{imp_num},{host_num}",
            '{{{About host HOST_INDEX}}}': node.get(f'about_host_{host_num}', 'Information about this host.'),
            '{{{About1 host HOST_INDEX}}}': node.get(f'about1_host_{host_num}', 'Additional details.'),
            '{{{About2 host HOST_INDEX}}}': node.get(f'about2_host_{host_num}', 'More information.'),
            '{{{link_list_host HOST_INDEX}}}': host_links,
            '{{{HOST_HOSTNAME}}}': host.get('hostname', ''),
            '{{{HOST_COMPUTER}}}': host.get('computer', ''),
            '{{{HOST_SYSTEM}}}': host.get('system', ''),
            '{{{HOST_STATUS}}}': host.get('status', ''),
        }

        # Apply host-specific replacements
        for placeholder, value in host_replacements.items():
            host_html = host_html.replace(placeholder, value)

        host_sections.append(host_html)

    # Combine all parts: template + host sections
    output_content = template_content

    # Replace the entire host section block with generated sections
    host_block = '<!-- BEGIN_HOST_SECTION -->' + host_section_template + '<!-- END_HOST_SECTION -->'
    generated_hosts = '\n'.join(host_sections)
    output_content = output_content.replace(host_block, generated_hosts)

    # Apply base replacements
    for placeholder, value in base_replacements.items():
        output_content = output_content.replace(placeholder, value)

    # Also replace the my_IMP_Number in the script
    output_content = re.sub(
        r'const my_IMP_Number = \d+;',
        f'const my_IMP_Number = {imp_num};',
        output_content
    )

    # Generate output filename
    output_filename = f'arpanet-node-{imp_num}-{name1}.html'
    output_path = Path(__file__).parent / output_filename

    # Write file
    with open(output_path, 'w') as f:
        f.write(output_content)

    print(f"✓ Created: {output_filename}")
    print(f"  IMP #{imp_num}: {name1}")
    if computers:
        for host_num in sorted(computers.keys()):
            host = computers[host_num]
            print(f"    Host {host_num}: {host.get('hostname', 'N/A')} ({host.get('computer', 'N/A')})")
    print()

print(f"\nSuccessfully generated {len(nodes)} ARPANET node pages!")
