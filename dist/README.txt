# SC4TerrainExtensions

This release zip includes:
- the Windows installer
- LICENSE.txt
- THIRD_PARTY_NOTICES.txt
- SC4TerrainExtensions.spdx.json
- SHA256SUMS.txt

Installation instructions:
- Install SC4RenderServices first so SC4RenderServices.dll is already present in your Plugins folder.
- SC4RenderServices project page: https://github.com/caspervg/sc4-render-services
- Install the Microsoft Visual C++ 2015-2022 Redistributable for x86: https://aka.ms/vs/17/release/vc_redist.x86.exe
- Run the included SC4 Terrain Extensions installer.
- The installer will ask for your SimCity 4 game root and Plugins folder, verify the SC4RenderServices dependency, and install the plugin files. If SC4RenderServices is missing or too old, the installer will stop and tell you where to download it.

Usage instructions:
- Use the terrain menu buttons for Flatten, Constant Grade, and Bridge Approach tools.
- You can also use the earthbender, bridgebuilder, terrainsnap and contour cheat commands.
- If something looks off, check <My Documents>/SimCity 4/SC4TerrainExtensions.log.
