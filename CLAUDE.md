# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Communication Style

**Be succinct and direct in all responses. Do not over-engineer solutions unless explicitly asked to do so. Keep explanations brief and focus on the specific task at hand.**

## Project Overview

This is a DLL plugin for SimCity 4 that provides terrain extension tools. The plugin creates custom terrain manipulation tools accessible through in-game cheat codes. It uses modern C++20 with a modular architecture for adding new terrain tools.

## Build System

**Prerequisites:**
- Visual Studio 2022
- Windows 10 or later
- Target platform: Win32 (32-bit)

**Building the plugin:**
```bash
# Open the solution in Visual Studio
# src/sc4-terrain-extensions.sln

# Build configurations available:
# - Debug|Win32
# - Release|Win32

# Post-build events automatically copy DLL to SimCity 4 plugins folder:
# Debug: C:\Users\caspe\Documents\SimCity 4\Plugins
# Release: C:\Users\caspe\OneDrive - Maplix\SimCity 4\Plugins
```

**Dependencies managed by vcpkg:**
- wil (Windows Implementation Library)
- args (command line argument parsing)

## Vendored Libraries

**gzcom-dll** (`vendor/gzcom-dll/`): Core SimCity 4 COM interface library providing:
- SimCity 4 system interfaces: `cISC4App`, `cISC4City`, `cISTETerrain`, `cISC4View3DWin`
- Graphics and UI: `cIGZWin*`, `cIGZCanvas`, `cIGZGraphicSystem`
- Message system: `cIGZMessage2*`, `cIGZMessageServer2`
- Persistence: `cIGZPersistResourceManager`, `cIGZFile`
- Utilities: `cRZBaseString`, `cRZAutoRefCount`, service pointers
- Math types: `SC4Rect`, `SC4Point`, `SC4Vector`

**EASTL** (`vendor/EASTL/`): High-performance STL replacement from EA
- Containers: vector, list, map, hash_map, fixed_* variants
- Algorithms and utilities
- Memory management with custom allocators

**EABase** (`vendor/EABase/`): Platform abstraction layer

**sc4-dll-basics** (`vendor/sc4-dll-basics/`): Common SC4 plugin utilities
- Logger, FileSystem, DebugUtil, SC4VersionDetection

## Architecture

**Core Components:**

1. **TerrainExtensionsDllDirector** (`src/TerrainExtensionsDllDirector.cpp`): Main plugin entry point that handles SimCity 4 integration, message processing, and cheat code management.

2. **TerrainToolRegistry** (`src/TerrainToolRegistry.hpp`): Registry system that manages all terrain tools, handles argument parsing, and executes appropriate tools.

3. **TerrainTool** (`src/TerrainTool.hpp`): Abstract base class for all terrain tools providing common terrain manipulation utilities, vertex operations, and coordinate validation.

4. **Tool Implementations**:
   - `ConstantGradeTool` (`src/ConstantGradeTool.cpp`): Creates paths with constant grade/slope
   - `SlopeMaker` (`src/SlopeMaker.cpp`): Creates smooth slopes between two points

**Plugin Integration:**
- Uses gzcom-dll framework for SimCity 4 COM integration
- Cheat code: "earthbender" followed by tool commands
- Integrates with SimCity 4's terrain system via cISTETerrain interface

**Tool Architecture Pattern:**
Each tool inherits from TerrainTool and implements:
- `RegisterArguments()`: Define command-line arguments using args library
- `ShouldExecute()`: Check if this tool should handle the parsed command
- `Execute()`: Perform the terrain manipulation
- Tool metadata methods (GetName, GetDescription, GetUsage)

## Development Workflow

**Adding a new terrain tool:**
1. Create new class inheriting from TerrainTool
2. Implement required virtual methods
3. Register tool in TerrainExtensionsDllDirector::SetUpTools()
4. Use TerrainTool base class utilities for terrain manipulation

**Testing:**
- Configure Visual Studio to launch SimCity 4 with debugging
- Use command line: `-intro:off -CPUcount:1 -w -CustomResolution:enabled -r1920x1080x32`
- In-game testing via cheat console: "earthbender [tool] [args]"

**Key Terrain Utilities (TerrainTool base class):**
- `GetTileCorners()`: Get vertex heights for a tile
- `SetAltitudeAtVertex()`: Modify terrain height with bounds checking
- `GetTileAverageHeight()`: Calculate average height of tile corners
- `ClampToTerrainBounds()`: Ensure coordinates are within terrain limits
- `Refresh()`: Update terrain display after modifications

## Code Conventions

- Modern C++20 features used throughout
- Uses COM smart pointers (cRZAutoRefCount, service pointers)
- Logging via Logger singleton with structured log levels
- Error handling with bounds checking for all terrain operations
- Memory management follows RAII principles with smart pointers