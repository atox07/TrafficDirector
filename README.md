# Traffic Director 0.1.1

C++ GTA V Story Mode ASI mod. English in-game menu. Requires Script Hook V and an ASI loader. No RAGE Plugin Hook, LSPDFR, ScriptHookVDotNet or menu DLL is required.

This is an initial implementation. It has not been compiled or tested in GTA V in the authoring environment. Build it using the manual workflow and test on an empty/free-roam session before relying on it for scenes. AI driving and road closures require in-game validation. No claim of verified Enhanced compatibility is made; install the Script Hook V runtime appropriate for your edition and game build.

## Build on GitHub (manual only)

1. Create a repository and upload **the contents of this directory** at its root, including `.github/workflows/build.yml`.
2. Open **Actions > Build Traffic Director (manual) > Run workflow**.
3. Select your branch and click **Run workflow**.
4. When the job finishes, download **TrafficDirector-Windows-x64** from Artifacts.

The workflow has only `workflow_dispatch`: pushing commits, creating pull requests and uploading files do not launch builds. The workflow must be on the default branch before GitHub shows its Run workflow button. If uploading through a web interface omits `.github`, add `.github/workflows/build.yml` manually with GitHub's Create new file.

The build uses Windows x64, CMake, C++17 and MSVC. It does not download third-party SDKs. MSVC generates an import library from `src/shv_imports.def`, which lists the official decorated C++ export names. Windows loads `ScriptHookV.dll` as a normal dependency before initializing the ASI. The workflow also checks all nine imports in the resulting ASI using dumpbin and includes `imports.txt` in the artifact.

Local alternative in a Visual Studio x64 developer terminal:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

## Install

1. Install the official Script Hook V runtime for your game build and its ASI loader (`dinput8.dll`): https://www.dev-c.enbdev.com/gtav/scripthookv/
2. Copy `TrafficDirector.asi` beside `GTA5.exe` (the main game directory).
3. Start Story Mode. The message **Traffic Director 0.1.1 loaded - Press K** should appear. Press **K** to open the menu.

The mod creates `TrafficDirector/TrafficDirector.dat` beside the ASI when you save. Keep the ASI filename unchanged. The runtime/loader are not included in this source package.

## 0.1.1 loading fix / diagnostics

Version 0.1 incorrectly looked up plain export names such as `nativeInit` with GetProcAddress. The official runtime exports MSVC C++ names such as `?nativeInit@@YAX_K@Z`, so the old initializer could return FALSE before registering the script. Version 0.1.1 uses normal DLL imports with the correct names and an explicit runtime dependency, removing the dynamic lookup and ASI load-order assumption.

K now uses Script Hook V's keyboard callback, ignores key-repeat and forwards the toggle to the script fiber. Other menu navigation retains polling. The ASI records initialization and each K toggle in **TrafficDirector.log** beside the ASI. No world natives run in the keyboard callback or DllMain.

If the menu still does not appear:

- No `TrafficDirector.log`: inspect `asiloader.log` and `ScriptHookV.log`; Windows/ASI loader did not initialize the plugin, the game directory is wrong, or the folder cannot be written.
- `ASI attached` but no `Script fiber started`: script registration did not progress; check `ScriptHookV.log` for runtime/game compatibility.
- `Initialization complete` but no `K received`: the game is not receiving the registered keyboard event, or another plugin is interfering.
- `K received: menu opened` but no menu: input works; investigate native drawing/runtime rather than the key binding.

This source correction is verified against published Script Hook V declarations/import names. It still requires a successful GitHub build and an in-game test; no local game execution or C++ build was available during authoring.

## Controls

| Key | Action |
| --- | --- |
| K | Open / close menu |
| Up / Down | Select row |
| Left / Right | Adjust value / cycle path |
| Enter | Activate row (numeric rows increase by one step) |
| Backspace / Escape | Previous page |

Names and vehicle model names use GTA's on-screen keyboard. Stand at the desired road position when placing collectors, waypoints and spawners. Avoid placing a spawner at your current occupied vehicle position; occupied spawn points wait until clear.

## Manage Paths

Create Path puts waypoint 0, the **collector**, at your location. Add at least one destination waypoint before collecting traffic. Enabled paths intercept ambient NPC drivers within the collector radius. Overlapping collectors are resolved in path-list order.

Path options: rename, enable/disable, add waypoint, remove last waypoint, edit each waypoint, delete path, assign nearest eligible driver within 40 metres. The manual driver action is integrated here instead of a separate Manage Drivers menu.

Waypoint options:

- Move Here.
- Collection Radius for waypoint 0; Arrival Radius for later waypoints, in metres.
- Speed Zone Radius: adopt that waypoint's speed when approaching within this distance.
- Waypoint Speed in km/h. Outside the next speed zone, keep the previous waypoint's speed.
- Stop Waypoint and Stop Duration in seconds.
- Drive Directly: request direct driving instead of normal road following. GTA AI/collisions still determine the actual motion.
- Delete individual non-collector waypoints.

Collector stop/direct flags are not executed: waypoint 0 is the entry point. Its speed sets the starting cruise speed. Drivers visit destinations from waypoint 1, then resume ambient wandering after the final waypoint. Disabling/deleting a path releases its drivers. Editing an individual waypoint list via Delete This Waypoint releases all current managed drivers to avoid stale indices.

## Manage Spawners

Create a spawner at your location and set:

- Name and GTA vehicle model name (e.g. `asea`, `police`, `adder`; add-on models must already be installed).
- Assigned path, which needs at least two points.
- Spawn interval in seconds, maximum active vehicles and heading.
- Position using Move Here; enabled state.

Each vehicle gets an NPC driver and follows the assigned path starting at destination waypoint 1. Place the spawner near the beginning of that path. Models load asynchronously, and the mod waits while the spawn point is occupied. Maximum Active Vehicles counts spawned vehicles still existing in the world, including those that have finished their path. Deleting a spawner does not delete its existing vehicles.

## Traffic Block Zones

Create Block Zone In Front places a finite box in front of your character, aligned with your heading. Adjust name, length, width, height, heading, center and enabled state. The preview displays the actual box. Dimensions are metres; height is centered on the zone's Z. The box stays fixed when the camera turns.

The primary mechanism is `SET_ROADS_IN_ANGLED_AREA`; disabling/deleting/moving a zone restores its old road volume and reapplies remaining zones, including overlaps. Enabled closures are periodically reapplied for streamed road nodes. No global vehicle density override is used.

**Important behavior:** closing road nodes can affect NPC routing, including your managed drivers, and can cause U-turns near the boundary. Close the unwanted branch, keeping the custom route outside the closed nodes where possible. Road-node closures do not guarantee suppression of parked-car generators, mission scripts or another mod's spawns. The mod does not globally disable those systems.

- **Clear Ambient Vehicles Once** removes eligible ambient cars already in the exact box.
- **Off-screen Ambient Cleanup**, disabled by default, checks every 100 ms and removes eligible ambient cars inside the box only when GTA reports them off-screen. This is a cleanup fallback after creation, not an interception before spawn. It may miss very brief appearances.
- Player vehicle (including as passenger), mission vehicles and vehicles currently tracked by this mod are protected. Cleanup removes the ambient driver as well. It does not remove every passenger separately.

Use Settings > Disable All Block Zones before unloading the plugin during play. Normal game shutdown naturally resets world state. The ASI cannot safely call world natives from Windows DLL-detach callbacks; external hot-unloading without first disabling zones may leave closures active until the session ends.

## Settings and persistence

Show World Preview, Automatic Collection, Release All Managed Drivers, Disable All Block Zones, Save Scene, Load Scene.

Saving records paths, waypoint settings, spawners, zones and settings in a versioned text `.dat` file with quoted names. Changes are saved explicitly with Save Scene; the last saved scene loads automatically at startup. Save while you want enabled spawners/closures to remain enabled on next startup. Reloading replaces the current configuration and releases current managed drivers; it does not delete their cars. Existing world entities are not serialized.

## Source map

- `src/main.cpp`: runtime bridge, traffic logic, editor/menu and persistence.
- `CMakeLists.txt`: Windows x64 ASI target, static MSVC runtime.
- `.github/workflows/build.yml`: manual Windows build and downloadable artifact.

Source is a new implementation; no Scene Manager DLL was decompiled or copied.
