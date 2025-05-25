# Aeonix Navigation
3D navigation plugin for Unreal Engine

## Overview
Aeonix Navigation is a voxel-based pathfinding system for Unreal Engine, designed for efficient and flexible 3D navigation. It leverages an octree data structure for multi-resolution spatial queries and fast pathfinding.

## Key Features
- Voxel-based 3D navigation with octree data structure
- Various pathfinding heuristic options for optimising against different environments
- Various path optimisation algorithms, including corridor-based string pulling using the funnel algorithm
- Editor tools for debugging, visualizing and configuring navigation data

## Modules
Aeonix Navigation is organized into several modules:

- **AeonixNavigation**: Core runtime module containing the main navigation logic, pathfinding algorithms, octree data structures, and public API. Source is under `Source/AeonixNavigation`.
    - `Private/`: Internal implementation (pathfinding, data, utilities, etc.)
    - `Public/`: Public interfaces and headers for integration.
- **AeonixEditor**: Editor integration module for Unreal Engine, providing tools and utilities for visualizing, configuring, and debugging navigation data. Source is under `Source/AeonixEditor`.
    - `Private/`: Editor tool implementations.
- **AeonixNavigationTests**: Unit and integration tests for the navigation system. Source is under `Source/AeonixNavigationTests`.
    - `Private/`: Test cases for core features and algorithms.

## Installation
1. Download and extract the plugin archive.
2. Place the extracted files into your project's `Plugins/AeonixNavigation` directory.
3. Regenerate project files and build the project.
4. Configure navigation parameters in `AeonixGenerationParameters.h`.
5. Use the provided API to query paths and perform navigation in your game logic.

## Main Files
- `AeonixPathFinder.cpp/h`: Pathfinding and string pulling algorithms
- `AeonixData.cpp/h`: Octree data structure and spatial queries
- `AeonixSubsystem.h/cpp`: Core Unreal Engine subsystem managing navigation data, agents, and pathfinding requests
- `AeonixEditorDebugSubsystem.h/cpp`: Editor subsystem for debugging and visualizing navigation data within the Unreal Editor
- `AeonixBoundingVolume.h/cpp`: Defines navigation volumes used for voxelization
- `AeonixNavAgentComponent.h/cpp`: Component for actors to participate in navigation
- `AeonixGenerationParameters.h`: Configuration for navigation data generation

These files define the core runtime, editor, and integration points for Aeonix Navigation. For more details, see in-source documentation and module structure above.

## Testing
Unit tests are located in `Source/AeonixNavigationTests`.

## Credits
Based on and extended from [UESVON](https://github.com/midgen/uesvon) by midgen.

## License
See LICENSE file for details.
