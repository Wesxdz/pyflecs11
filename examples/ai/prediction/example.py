import flecs
import torch
import graphviz
import colorsys

def generate_distinct_colors(n):
    """Generate n visually distinct colors"""
    colors = []
    for i in range(n):
        hue = i / n
        saturation = 0.8
        value = 0.9
        rgb = colorsys.hsv_to_rgb(hue, saturation, value)
        hex_color = "#{:02x}{:02x}{:02x}".format(
            int(rgb[0] * 255),
            int(rgb[1] * 255),
            int(rgb[2] * 255)
        )
        colors.append(hex_color)
    return colors

def create_simple_world():
    """Create a simple world with some entities and relationships"""
    world = flecs.World()
    
    # Create entities
    player = world.entity("Player")
    companion = world.entity("Companion") 
    enemy = world.entity("Boss")
    town = world.entity("Town")
    sword = world.entity("Sword")
    
    # Add relationships
    player.add("Friend", companion)
    player.add("Enemy", enemy)
    player.add("Location", town)
    player.add("HasItem", sword)
    companion.add("Friend", player)
    companion.add("Location", town)
    
    return world

def visualize_flecs_graph(world, output_name="flecs_graph"):
    """Create a visual graph from Flecs world data"""
    
    # Export graph data from Flecs
    print("🔍 Exporting graph data from Flecs...")
    graph_data = world.export_graph_numpy()
    
    # Convert to PyTorch tensors
    x = torch.from_numpy(graph_data['node_ids']).unsqueeze(1)  # Add feature dimension
    edge_index = torch.from_numpy(graph_data['edge_index'])
    edge_attr = torch.from_numpy(graph_data['edge_relations'])
    
    # Create name mapping from the lists
    node_names = list(graph_data['node_names'])
    relation_names = list(graph_data['relation_names'])
    
    # Create mapping from node IDs to names
    name_map = {}
    for i, node_id in enumerate(graph_data['node_ids']):
        name_map[str(node_id)] = node_names[i]
    
    # Create mapping from relation IDs to names  
    relation_name_map = {}
    for i, rel_id in enumerate(graph_data['edge_relations']):
        if str(rel_id) not in relation_name_map:
            # Find the relation name - need to map back from edge data
            rel_idx = i if i < len(relation_names) else 0
            relation_name_map[str(rel_id)] = relation_names[rel_idx] if relation_names else str(rel_id)
    
    print(f"   Nodes: {graph_data['num_nodes']}")
    print(f"   Edges: {graph_data['num_edges']}")
    print(f"   Node names: {node_names}")
    print(f"   Relation names: {relation_names}")

    # Create the graph
    dot = graphviz.Digraph(comment='Flecs Entity Relationship Graph')

    # Global graph attributes
    dot.attr(
        bgcolor='#24292e',  # Dark background
        rankdir='LR',
        splines='true',
        overlap='false',
        concentrate='true',
        fontname='Helvetica'
    )

    # Base colors
    colors = {
        'entity': '#f57d4a',      # Orange for regular entities
    }

    # First, collect relationships grouped by type AND target
    relationships = {}  # key: (rel_type, target_idx)
    if edge_index.size(1) > 0:
        for i, (s, t) in enumerate(edge_index.t()):
            rel_type = int(edge_attr[i].item())
            src_idx = int(s)
            tgt_idx = int(t)
            key = (rel_type, tgt_idx)  # Group by both relationship type and target
            if key not in relationships:
                relationships[key] = []
            relationships[key].append(src_idx)

    # Generate distinct colors for each relationship type
    unique_rel_types = torch.unique(edge_attr).tolist()
    rel_colors = {rel_type: color 
                  for rel_type, color in zip(unique_rel_types, 
                                           generate_distinct_colors(len(unique_rel_types)))}

    # Add entity nodes
    for i in range(x.shape[0]):
        entity_id = int(x[i][0])
        name = name_map.get(str(entity_id), str(entity_id))
        label = f"{name}\n({entity_id})"
        
        # Check if this entity is a relationship type
        if entity_id in rel_colors:
            # This is a relationship type entity
            dot.node(
                f"entity_{i}",
                label,
                shape='diamond',
                style='',
                color=rel_colors[entity_id],
                fontcolor=rel_colors[entity_id],
                margin='0.2',
                width='1.0',
                height='1.0'
            )
        else:
            # Regular entity
            dot.node(
                f"entity_{i}",
                label,
                shape='circle',
                style='',
                color=colors['entity'],
                fontcolor=colors['entity']
            )

    # Add relationship nodes and their connections
    for (rel_type, tgt_idx), src_indices in relationships.items():
        rel_color = rel_colors[rel_type]
        rel_name = relation_name_map.get(str(rel_type), str(rel_type))
        
        # Create a unique node for this relationship type + target combination
        rel_node_id = f"rel_{rel_type}_{tgt_idx}"
        rel_label = f"{rel_name}\n({rel_type})"
        
        # Add the relationship node
        dot.node(
            rel_node_id,
            rel_label,
            shape='cds',
            style='',
            color=rel_color,
            fontcolor=rel_color,
            fillcolor=rel_color,
            margin='0.3,0.2',
            width='1.5',
            height='0.8'
        )
        
        # Add edges for all sources to this relationship node
        for src_idx in src_indices:
            dot.edge(
                f"entity_{src_idx}",
                rel_node_id,
                color=rel_color,
                penwidth='2'
            )
        
        # Add edge from relationship node to target
        dot.edge(
            rel_node_id,
            f"entity_{tgt_idx}",
            color=rel_color,
            penwidth='2'
        )

    # Save and render
    output_file = f'{output_name}_graph'
    dot.render(output_file, format='png', cleanup=True)
    print(f"✅ Graph saved as '{output_file}.png'")

    # Print debug info
    print(f"\n📊 Graph Statistics:")
    print(f"   Number of entities: {x.shape[0]}")
    print(f"   Number of unique relationship instances: {len(relationships)}")
    print(f"\n🔗 Relationship instances:")
    for (rel_type, tgt_idx), src_indices in relationships.items():
        tgt_id = int(x[tgt_idx][0])
        tgt_name = name_map.get(str(tgt_id), str(tgt_id))
        rel_name = relation_name_map.get(str(rel_type), str(rel_type))
        for src_idx in src_indices:
            src_id = int(x[src_idx][0])
            src_name = name_map.get(str(src_id), str(src_id))
            print(f"   {src_name} --[{rel_name}]--> {tgt_name}")

def main():
    print("🎮 Flecs Graph Visualization Demo")
    print("=" * 40)
    
    # Create sample world
    world = create_simple_world()
    
    # Visualize the graph
    visualize_flecs_graph(world, "flecs_rpg_world")
    
    print(f"\n🎯 Done! Check the generated PNG file.")

if __name__ == "__main__":
    main()