#!/usr/bin/env python3
"""
ARPANET 1973 Interactive Network Topology Visualizer (PyVis)
Creates an interactive, draggable network visualization in the browser
"""

from pyvis.network import Network
import webbrowser
import os

# Import functions from the main script
from topol import parse_arpanet_nodes, validate_connections, build_network_graph

def create_interactive_network(G, node_names, nodes_data):
    """
    Create an interactive PyVis network with fixed corner nodes and original x,y coordinates
    """
    # Build a map of node number to original coordinates
    node_coords = {}
    for node_data in nodes_data:
        if 'x' in node_data and 'y' in node_data:
            node_coords[node_data['node']] = (node_data['x'], node_data['y'])
    # Create PyVis network
    net = Network(
        height='900px',
        width='100%',
        bgcolor='#FFFFFF',
        font_color='#000000',
        heading='ARPANET Network Topology (1973) - Interactive'
    )

    # Configure physics for better layout
    net.set_options('''
    {
      "physics": {
        "enabled": true,
        "stabilization": {
          "enabled": true,
          "iterations": 200
        },
        "barnesHut": {
          "gravitationalConstant": -8000,
          "centralGravity": 0.3,
          "springLength": 200,
          "springConstant": 0.04,
          "damping": 0.09,
          "avoidOverlap": 0.5
        }
      },
      "interaction": {
        "hover": true,
        "tooltipDelay": 100,
        "navigationButtons": true,
        "keyboard": true
      }
    }
    ''')

    # Define fixed nodes and their positions
    fixed_nodes_config = {
        'SRI': {'x': -800, 'y': -600, 'color': '#FF6B6B'},      # Top left
        'MIT': {'x': 800, 'y': -600, 'color': '#FF6B6B'},       # Top right
        'ETAC': {'x': 800, 'y': 600, 'color': '#FF6B6B'},       # Bottom right
        'USC-ISI': {'x': -800, 'y': 600, 'color': '#FF6B6B'},   # Bottom left
        'DOCB': {'x': 0, 'y': 0, 'color': '#FF6B6B'}            # Center
    }

    name_to_node = {name: num for num, name in node_names.items()}

    # Add nodes with appropriate settings
    nodes_with_coords = 0
    nodes_without_coords = 0

    for node_num in G.nodes():
        node_name = node_names[node_num]

        if node_name in fixed_nodes_config:
            # Fixed node - red, locked position (manually set)
            config = fixed_nodes_config[node_name]
            net.add_node(
                node_num,
                label=node_name,
                title=f"{node_name} (Node #{node_num}) - FIXED POSITION",
                color=config['color'],
                size=25,
                x=config['x'],
                y=config['y'],
                physics=False,  # Disable physics so it stays fixed
                fixed={'x': True, 'y': True},
                font={'size': 14, 'color': '#FFFFFF', 'face': 'arial', 'bold': True}
            )
        elif node_num in node_coords:
            # Node with original coordinates - use them but allow dragging
            x_orig, y_orig = node_coords[node_num]
            # Scale coordinates (original are in ~0-1177 x 0-879 range)
            x_scaled = (x_orig - 588.5) * 1.5  # Center and scale
            y_scaled = (y_orig - 439.5) * 1.5  # Center and scale
            net.add_node(
                node_num,
                label=node_name,
                title=f"{node_name} (Node #{node_num}) - Original position, drag to move",
                color='#4ECDC4',
                size=20,
                x=x_scaled,
                y=y_scaled,
                font={'size': 12, 'color': '#FFFFFF', 'face': 'arial'}
            )
            nodes_with_coords += 1
        else:
            # Regular node - teal, draggable, no initial position
            net.add_node(
                node_num,
                label=node_name,
                title=f"{node_name} (Node #{node_num}) - Drag to move",
                color='#4ECDC4',
                size=20,
                font={'size': 12, 'color': '#FFFFFF', 'face': 'arial'}
            )
            nodes_without_coords += 1

    print(f"   - {nodes_with_coords} nodes using original x,y coordinates")
    print(f"   - {nodes_without_coords} nodes positioned by physics engine")

    # Add edges
    for edge in G.edges():
        net.add_edge(edge[0], edge[1], color='#666666', width=2)

    return net

def main():
    """
    Main function to create interactive ARPANET visualization
    """
    # Path to the JavaScript file
    js_file = '../../arpa/assets/js/arpanet-nodes.js'
    output_file = 'arpanet_network_interactive.html'

    print("ARPANET Interactive Network Topology Visualizer (PyVis)")
    print("=" * 60)

    # Parse the data
    print(f"\n1. Parsing data from: {js_file}")
    nodes = parse_arpanet_nodes(js_file)
    print(f"   Found {len(nodes)} IMP nodes")

    # Validate connections
    print("\n2. Validating modem connections...")
    errors = validate_connections(nodes)
    if errors:
        print(f"   ❌ Found {len(errors)} connection error(s):")
        for error in errors:
            print(error)
        print("\n" + "=" * 60)
        print("ERROR: Data validation failed!")
        print("All modem connections must be bidirectional.")
        print("Please fix the data errors before visualizing.")
        print("=" * 60)
        return  # Exit without displaying the graph
    else:
        print("   ✓ All modem connections are bidirectional and valid")

    # Build the network graph
    print("\n3. Building network graph...")
    G, node_names = build_network_graph(nodes)
    print(f"   Graph has {G.number_of_nodes()} nodes and {G.number_of_edges()} edges")

    # Create interactive visualization
    print("\n4. Creating interactive visualization...")
    print("   Fixed nodes (red, cannot be moved):")
    print("   - SRI (top left)")
    print("   - MIT (top right)")
    print("   - ETAC (bottom right)")
    print("   - USC-ISI (bottom left)")
    print("   - DOCB (center)")
    print("\n   Other nodes (teal, draggable):")
    print("   - Using original x,y coordinates where available")
    print("   - Drag with mouse to reposition")
    print("   - Hover for details")
    print("   - Scroll to zoom")

    net = create_interactive_network(G, node_names, nodes)

    # Generate HTML file
    print(f"\n5. Generating HTML file: {output_file}")
    net.save_graph(output_file)

    # Get absolute path and open in browser
    abs_path = os.path.abspath(output_file)
    print(f"\n✓ Interactive visualization created!")
    print(f"   File: {abs_path}")
    print(f"\n   Opening in default browser...")

    webbrowser.open('file://' + abs_path)

    print("\n" + "=" * 60)
    print("CONTROLS:")
    print("  • Drag nodes to move them (except red fixed nodes)")
    print("  • Scroll to zoom in/out")
    print("  • Click and drag background to pan")
    print("  • Hover over nodes for details")
    print("  • Use navigation buttons in the visualization")
    print("=" * 60)

if __name__ == '__main__':
    main()
