<h1 align="center">Portal 2 RTX Remix Compatibility Mod</h1>

<div align="center" markdown="1"> 

This client modification is specifically made for nvidia's [rtx-remix](https://github.com/NVIDIAGameWorks/rtx-remix).  
How does a shader based game work with remix? By manually reimplementing fixed function rendering :) 


<div align="center" markdown="1">

<img src=".github/img/logo.png" alt="Description" width="60%">
</div>



__WIP__ & __Please Note:__  
RTX Remix was never intented to support Portal 2.  
If you encounter crashes, broken things or similar, it's either due to incompatibility,  
the limits of fixed function rendering or due to bugs in the compatibility mod itself.  


### This is not trying to be a remaster.
It simply makes the game compatible with RTX Remix.  
Please keep that in mind.

</div>

<br>
<br>

<div align="center" markdown="1">

![img](.github/img/01.png)
![img](.github/img/04.png)
</div>

<br>

<div align="center" markdown="1">

### __[ Remix Compatibility Features ]__   
🔹Most things are rendered using the fixed-function pipeline🔹  
🔹Remix friendly culling and the ability to manually override culling🔹  
🔹Ability to spawn and animate lights on events using a keyframe system🔹  
🔹Per map loading of remix config files to set remix variables🔹  
🔹Ability to animate remix variables on events🔹  
🔹Spawning of unique anchor meshes🔹  
🔹... and much, much more ...🔹  

<br>
<br>

If you want to support my work:

<a href="https://patreon.com/xoxor4d"><img src=".github/img/patreon.png" width="12%"></a>  
<a href="https://ko-fi.com/xoxor4d"><img src=".github/img/kofi.png" width="10%"></a>

</div>


## Installation / Usage
- Download the latest [release](https://github.com/xoxor4d/p2-rtx/releases) and follow instructions found __there__.

<br>

#### ✳️ Info: 
- Take a look at the [Wiki](https://github.com/xoxor4d/p2-rtx/wiki/Compatibility-Mod-Feature-Guide) for in-depth guides on features that come with the compatibility mod 🍓
  
- Current releases ship with:
  - [custom build of the remix-dxvk runtime]([https://github.com/xoxor4d/dxvk-remix/tree/game/p2](https://github.com/xoxor4d/dxvk-remix/tree/game/p2_rebase2)) which includes necessary changes  
for Portal 2 (`bin/.trex/d3d9.dll`)
  - [custom build of bridge-remix](https://github.com/xoxor4d/bridge-remix/tree/pr/remixapi-add-remove-texhash) which includes necessary changes  
for Portal 2 (`bin/.trex/NvRemixBridge.exe` & `bin/d3d9.dll`)

- Some engine tweaks that are required to make the game compatible result in CPU bottlenecks on some maps (software skinning instead of HW skinning). This may or may not improve in future updates.

<br>

## ⚠️ Troubleshooting (click to expand):

<details><summary>Do I need to start the game from the batch file every time?</summary>
<br>

1. No, copy all of the commandline arguments of the batch file (everything after `START portal2.exe`)
   
2. Add them as launch args by right clicking Portal 2 in Steam -> Properties -> Launch Options

3. Sart the game from Steam

- Note: starting from Steam skips the self-repair check in `run-p2-rtx.bat`. When a Steam update for Portal 2 comes out (Steam restores the vanilla `portal2.exe`, which disables the mod), run `run-p2-rtx.bat` once to repair the install.

<br></details>


<details><summary>The mod stopped working after a Steam update</summary>
<br>

- Steam updates (and "verify integrity of game files") restore the vanilla `portal2.exe`. That removes the modification that loads `p2-rtx.dll`, so the game starts without the mod.

- Run `run-p2-rtx.bat` - it detects the vanilla exe and restores the patched `portal2.exe` automatically (the vanilla exe is kept as `portal2.exe.vanilla`).

- Alternatively: re-run the installer or extract the release zip over the game folder again.

- Advanced: [`tools/patch_portal2_imports.py`](tools/patch_portal2_imports.py) re-applies the import patch to any vanilla `portal2.exe` (useful if the shipped exe ever goes out of date after a game update).

<br></details>


<details><summary>How do I disable remix?</summary>
<br>

- Run `toggle-p2-rtx.bat` and follow the instructions to disable or enable the mod
- There are a few game cvars you might need to reset after you've disabled the compatibility mod.  
```
r_portal_stencil_depth 2
r_dopixelvisibility 1
mat_fullbright 0
mat_softwareskin 0
mat_fastnobump 0
mat_normalmaps 1
cl_tlucfastpath 1
cl_modelfastpath 1
cl_brushfastpath 1  
```

<br></details>


<details><summary>Crashing, startup issues, not working ..</summary>
<br>

- Make sure that you have no clipping software such as "medal" running in the background

- If your game is installed within `Program Files`:
  - right click `portal2.exe` and click settings - compatibility tab and enable run as admin
  - or install the game somewhere outside `Program Files`

- Other startup issues?
  - Add `-debug` as a launch arg to `run-p2-rtx.bat` (`START portal2.exe -debug -insecure ...`)
  - An external console window should show up when launching the game, copy and paste it's contents when you create an issue on GitHub

- Download and install [DirectX End-User Runtimes (June 2010)](https://www.microsoft.com/en-ie/download/details.aspx?id=8109)

<br></details>


<details><summary>Portals not showing up - Darkness</summary>
<br>

- Make sure that you installed the [base-remix-mod](https://github.com/xoxor4d/p2-rtx-base-mod) as stated in the release notes

<br></details>


<details><summary>No sound</summary>
<br>

- Either copy `_master.cache` from `root/portal2/maps/soundcache` to `root/portal2_dlc3/maps/soundcache` 
- Or paste this into the in-game console and execute: `snd_rebuildaudiocache;snd_updateaudiocache;exit`

<br></details>

<details><summary>Game is too dark, sunlight is leaking on certain maps, textures look flat</summary>
<br>

- This is not an issue with the compatibility mod itself. It's up to the people modding the game using the remix toolset to place proper lights and overhaul textures and meshes.

<br></details>


- Other Issue?
> - Look at [Closed Issues](https://github.com/xoxor4d/p2-rtx/issues?q=is%3Aissue+is%3Aclosed) or [Discussions](https://github.com/xoxor4d/p2-rtx/discussions) to see if people had similar issues

<br>

##  Credits
- [Nvidia - RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix)
- [People of the showcase discord](https://discord.gg/j6sh7JD3v9) - especially the nvidia engineers ✌️
- All early access people for testing/bug reporting and for covering my electricity bill ⚡
- [Wolƒe Strider Shoσter](https://github.com/wolfestridershooter) - for all the high quality bug reports! 
- Yosuke Nathan - Portal 2 Remix Logo
- [dear-imgui](https://github.com/ocornut/imgui)
- [imgui-blur-effect](https://github.com/3r4y/imgui-blur-effect)
- [minhook](https://github.com/TsudaKageyu/minhook)
- [toml11](https://github.com/ToruNiina/toml11)

<br>

<div align="center" markdown="1">

![img](.github/img/02.png)
![img](.github/img/03.png)
![img](.github/img/05.png)
![img](.github/img/06.png)
</div>
