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

## SimCity 4 Interface Debugging

**Note: The gzcom-dll headers are reverse-engineered and may contain inaccuracies. When encountering interface-related crashes, verify method signatures and vtable order using Ghidra on the Mac binary (which has symbols) or Windows binary.**

### Common Interface Issues and Solutions:

1. **Missing Virtual Methods**:
   - **Symptoms**: Access violations, ESP errors, "privileged instruction" crashes
   - **Cause**: Missing virtual methods cause vtable misalignment
   - **Solution**: Check vtable order in Ghidra against header file
   - **Example**: `cISC4View3DWin::PickOccupant` was missing, causing `SetCursorText` to call wrong function

2. **Incorrect Method Signatures**:
   - **Symptoms**: ESP corruption, stack misalignment after method calls
   - **Cause**: Wrong parameter types (references vs pointers) or counts
   - **Solution**: Compare decompiled method signatures with header declarations
   - **Example**: `SetCursorText` expects `cIGZString const*` not `cIGZString const&`

3. **Input Control Implementation**:
   - **Object Layout**: Must match SC4's memory layout (use decompiled struct layouts)
   - **Calling Conventions**: Use `__thiscall` for SC4 virtual methods
   - **Cursor Setup**: Requires valid cursor IDs (e.g., `0xa16f1463` from existing tools)

### Debugging Resources:
- **Mac Binary**: Has symbols, easier to analyze vtables and method signatures
- **Windows Binary**: Symbol-less but matches target platform exactly
- **Ghidra Analysis**: Essential for verifying interface accuracy when crashes occur

## SimCity 4 Selection System Architecture

**SC4 provides a built-in selection visualization system accessible through the terrain interface.**

### Selection System Components:

1. **Terrain System (`cSTETerrain`)**:
   - Method `GetView()` (vtable offset `0x108`) returns terrain view interface
   - Returns `*(terrain + 0xe8) + 0xc` - points to selection interface within TerrainView3D

2. **TerrainView3D (`cSTETerrainView3D`)**:
   - Multi-interface object providing terrain rendering and selection
   - Selection interface accessible via `terrain->GetView()`
   - Key selection methods:
     - `MarkSelected()` (multiple overloads) - Draw selection overlays
     - `ClearCurrentSelections()` - Remove all visual selections
     - `GetOverlayManager()` - Access overlay management system

### Drag-to-Select Pattern (from `cSC4ViewInputControlLevelTerrain`):

**Standard SC4 area selection workflow:**

1. **Mouse Down**: Convert screen to terrain coordinates, store start position, set capture
2. **Mouse Move**: Update current position, compute selection bounds, update visual selection
3. **Mouse Up**: Finalize selection, execute operation, clear selection, release capture

**Key Implementation Details:**
- **Coordinate Conversion**: Screen coordinates → terrain tiles via division by tile scale factor
- **Bounds Computation**: Always compute proper min/max regardless of drag direction
- **Mouse Capture**: Use `SetCapture()`/`ReleaseCapture()` for reliable drag tracking
- **Visual Feedback**: Real-time selection rectangle updates during drag
- **Deferred Execution**: Perform operation only on mouse up, not during drag

### Member Layout Pattern (cSC4ViewInputControlLevelTerrain):
```cpp
// Offset 0x28: Terrain system pointer (SL::spTerrain)
// Offset 0x2c: Selection renderer pointer (from GetView())
// Offset 0x30: Boolean dragging flag
// Offset 0x34, 0x38: Start coordinates (X, Z)
// Offset 0x3c, 0x40: Current coordinates (X, Z)  
// Offset 0x44-0x50: Computed rectangle bounds (min_x, min_z, max_x, max_z)
// Offset 0x54: Terrain tile scale factor
```

### Selection Rendering:
```cpp
// Get selection renderer from terrain
auto* selectionRenderer = terrain->GetView();

// Draw rectangle selection (called during mouse move)
selectionRenderer->MarkSelected(bounds, 2, 1);  // bounds, type=2, visible=1

// Clear selection (called on completion)
selectionRenderer->ClearCurrentSelections();
```

This architecture provides consistent, performant selection visualization for all SC4 terrain tools.
- Memorize the learnings from cSC4ViewInputControlLevelTerrain please :)