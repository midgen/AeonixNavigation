# Aeonix Navigation

A voxel-based 3D pathfinding plugin for Unreal Engine 5.5, 5.6, and 5.7.

![GitHub last commit](https://img.shields.io/github/last-commit/midgen/AeonixNavigation?style=for-the-badge&logo=unrealengine)![GitHub branch status](https://img.shields.io/github/checks-status/midgen/AeonixNavigation/main?style=for-the-badge&logo=unrealengine)

Build validation via TeamCity CI/CD using the [Aeonix Demo](https://github.com/midgen/AeonixDemo) project.

## What is it?

Aeonix Navigation provides 3d navigation generation and pathfinding for Unreal Engine 5.6+ 

Main Features:
- Sparse Voxel Octree for excelent memory efficient in sparse environment
- A* pathfinding with various heuristics for optimising queries
- A number of post-processing functions for smooth paths
- *EXPERIMENTAL* Dynamic modifier regions that provide runtime updates of selected regions with no re-allocation of navigation data
- Editor tools for visualization and debugging

## Installation

1. Place the plugin in your project's `Plugins/AeonixNavigation` directory
2. Regenerate project files and build
3. Enable the plugin in the Plugins menu

## Getting Started

### 1. Create Navigation Volume

- Place an **AeonixBoundingVolume** in your level (Place Actors → Aeonix Bounding Volume)
- Scale it to encompass your desired navigable space
- Select it and configure in the Details panel:
  - **Octree Depth**: Controls octree subdivision depth. Higher values create finer detail. Use 5-6 for human-sized characters
  - **CollisionChannel**: Which geometry blocks navigation (usually ECC_WorldStatic)
  - **GenerationStrategy**: "Use Baked" (pre-generate) or "Generate OnBeginPlay" (runtime)
  - (Optional) Check 'Debug Leaf Voxels' to render blocked voxels (may need to increase the debug render distance) 
- Click **Generate** button to create navigation data

### 2. Add Component to AI

- Open your AI Controller or Character Blueprint
- Add **AeonixNavAgentComponent**
- Configure pathfinder settings

### 3. Use in Behavior Tree

- Add **BTTask_AeonixMoveTo** task to your Behavior Tree
- Set the **BlackboardKey** to your target location (Vector or Actor)
- Configure **AcceptableRadius** (how close AI needs to get, typically 100-200cm)

### 4. Testing

Use **AAeonixPathDebugActor** (editor-only) to test pathfinding:
- Place two AeonixPathDebugActors in your level
- Set one to START, one to END
- Move them around to visualize paths in real-time

## Editor Tools

**Aeonix Navigation Panel**: Window → Aeonix Navigation
- View and manage all navigation volumes in the level
- Access volume settings and regeneration controls

**Bounding Volume Details**: Custom details panel when selecting a volume
- Quick access to generation parameters
- Generate button for creating/updating navigation
- Visualization toggles for debugging

## Module Structure

- **AeonixNavigation**: Core runtime module (pathfinding, octree, subsystem)
- **AeonixEditor**: Editor integration (tools, visualization, debug actors)
- **AeonixNavigationTests**: Unit and integration tests

## Key Classes

- `AAeonixBoundingVolume`: Defines navigable 3D space
- `UAeonixNavAgentComponent`: Enables AI to use navigation
- `UAeonixSubsystem`: World subsystem managing navigation and pathfinding
- `AeonixPathFinder`: A* pathfinding implementation
- `UBTTask_AeonixMoveTo`: Behavior Tree movement task
- `AAeonixPathDebugActor`: Editor debug actor for testing

## Documentation

More indepth documentation to come. Please feel free to raise issues on github.

## Testing

Unit tests are in `Source/AeonixNavigationTests`. Example levels are in [AeonixDemo](https://github.com/midgen/AeonixDemo) project.

## Credits

Based on [UESVON](https://github.com/midgen/uesvon) by midgen (me).

## License

See LICENSE file.
