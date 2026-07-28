import osmnx as ox
import networkx as nx
import matplotlib.pyplot as plt
import geopandas as gpd
from shapely.geometry import Point, LineString

# Get the street network of Kowloon, Hong Kong
place_name = "Sham Shui Po District, Hong Kong"
graph = ox.graph_from_place(place_name)

# Project the graph to UTM (for accurate distance measurements)
graph_proj = ox.project_graph(graph)

# Convert the graph to an undirected graph (MST works on undirected graphs)
undirected_graph = graph_proj.to_undirected()

# Calculate the Minimum Spanning Tree using NetworkX
mst = nx.minimum_spanning_tree(undirected_graph, weight='length', algorithm='prim')

# Get building footprints using the updated features_from_place function
buildings = ox.features_from_place(place_name, tags={'building': True})

# Project the buildings to the same coordinate system as the graph
buildings_proj = ox.project_gdf(buildings, to_crs=graph_proj.graph['crs'])

# Create nodes at the centroids of the buildings
building_centroids = buildings_proj.centroid

# Lists to store connecting lines, red nodes (nearest road nodes), and magenta nodes (junctions)
connecting_lines = []
red_nodes = []
magenta_nodes = []
# Iterate over centroids, find nearest road node, and store as red nodes
for centroid in building_centroids:
    if centroid.is_empty:  # Skip any empty centroids
        continue
    # Find nearest road node to the centroid
    nearest_node = ox.distance.nearest_nodes(graph_proj, centroid.x, centroid.y)
    
    # Get the coordinates of the nearest road node
    nearest_node_coords = graph_proj.nodes[nearest_node]['x'], graph_proj.nodes[nearest_node]['y']
    
    # Create a LineString between the centroid and the nearest road node
    line = LineString([centroid, Point(nearest_node_coords)])
    connecting_lines.append(line)
    
    # Check if the nearest road node is a magenta node (a junction)
    if Point(nearest_node_coords) in magenta_nodes:
        # If it is a magenta node, reclassify it as a red node (nearest node to a green node cannot be magenta)
        magenta_nodes.remove(Point(nearest_node_coords))
        red_nodes.append(Point(nearest_node_coords))  # Store as a red node
    else:
        # If it's not a magenta node, store as a red node
        red_nodes.append(Point(nearest_node_coords))

# Identify road junctions (nodes with degree > 2)
# Remove any junctions that have been converted to red nodes already
for node, degree in dict(undirected_graph.degree()).items():
    node_coords = Point(graph_proj.nodes[node]['x'], graph_proj.nodes[node]['y'])
    if degree > 2 and not any(node_coords.equals(red_node) for red_node in red_nodes):  # Check with .equals() method
        magenta_nodes.append(node_coords)


# Remove non-red, non-green leaf nodes from MST
def remove_non_red_green_leaves(mst, red_nodes, green_nodes):
    red_nodes_set = set(red_nodes)  # Convert red nodes to set for faster lookup
    green_nodes_set = set(green_nodes)  # Convert green nodes to set for faster lookup
    nodes_removed = True

    while nodes_removed:
        nodes_removed = False
        leaf_nodes = [node for node in mst.nodes if mst.degree(node) == 1]  # Find leaf nodes
        
        for leaf in leaf_nodes:
            leaf_coords = Point(graph_proj.nodes[leaf]['x'], graph_proj.nodes[leaf]['y'])
            
            # Only remove the leaf node if it's not in red_nodes or green_nodes
            if leaf_coords not in red_nodes_set and leaf_coords not in green_nodes_set:
                # Remove the edge connected to this leaf node
                neighbors = list(mst.neighbors(leaf))
                if neighbors:
                    mst.remove_edge(leaf, neighbors[0])
                # Remove the leaf node itself
                mst.remove_node(leaf)
                nodes_removed = True

    return mst


# Plot the remaining MST (second plot only)
def plot_remaining_mst(remaining_mst, graph_proj, buildings_proj, building_centroids, connecting_lines, red_nodes):
    fig, ax = plt.subplots(figsize=(10, 10))

    # Plot the original road network (in gray)
    ox.plot_graph(graph_proj, ax=ax, node_color="none", edge_color="gray", show=False, close=False)

    # Plot the remaining MST (in blue) after leaf removal
    ox.plot_graph(remaining_mst, ax=ax, node_color="none", edge_color="blue", edge_linewidth=1.5, show=False, close=False)
    ax.set_title("MST route map of " + place_name)

    # Plot the buildings with transparency
    buildings_proj.plot(ax=ax, facecolor="khaki", edgecolor="black", alpha=0.6)
    # Plot the building centroids as green nodes
    gpd.GeoSeries(building_centroids).plot(ax=ax, color="green", markersize=2)
    # Plot the connecting lines
    connecting_lines_gdf = gpd.GeoDataFrame(geometry=connecting_lines)
    connecting_lines_gdf.plot(ax=ax, color="blue", linewidth=1)
    # Plot the red nodes (nearest road nodes to centroids) as red dots
    red_nodes_gdf = gpd.GeoDataFrame(geometry=red_nodes)
    red_nodes_gdf.plot(ax=ax, color="red", markersize=5)

    plt.show()


# Remove non-red, non-green leaf nodes and plot the remaining MST
remaining_mst = remove_non_red_green_leaves(mst.copy(), red_nodes, building_centroids)
plot_remaining_mst(remaining_mst, graph_proj, buildings_proj, building_centroids, connecting_lines, red_nodes)
