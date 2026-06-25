# Stormworks Camera Extractor (Separate Window Mod)

Language / Язык: **English 🇬🇧** | [Русский 🇷🇺](readmeru.md)

---

A modification for **Stormworks: Build and Rescue** that intercepts in-game monitor textures and redirects the camera feeds into a standalone, separate Windows window. This allows you to easily move your vehicle's cameras or displays to a second monitor.

---

## 🍕 Support the Project / I really need money to buy food

Developing and reversing game code takes a lot of time, effort, and late nights. If this tool helped you or saved your multi-monitor setup, **I really need your support to keep going and buy some food!** Any donation is deeply appreciated.

| **ETH (Ethereum / ERC-20)** | **USDT (ERC-20)** | **TRX (Tron Network)** |
| :---: | :---: | :---: |
| `0xC280Eb895795B1058C8A671eD79aDBFe14c7F71C` | `0xC280Eb895795B1058C8A671eD79aDBFe14c7F71C` | `TU27CurhG86YXTVyeYPWqZYLd9NMfAMWAD` |
| ![ETH QR](eth.jpg) | ![USDT QR](usdt.jpg) | ![TRX QR](trx.jpg) |

---

## 📸 Guide & Screenshots

### Step 1. Visual Studio Project Setup
To compile this project, you need to create a `Dynamic-Link Library (DLL)` project in Visual Studio. In the project properties (`Configuration Properties -> VC++ Directories`), make sure to include the paths for both **MinHook** and **Kiero** headers and libraries.

![Visual Studio Project Setup](1.jpg)

### Step 2. Initializing the Graphics Hook (OpenGL)
The code initializes the Kiero library targeting OpenGL, and MinHook hooks the original `wglSwapBuffers` function. This allows the program to capture the framebuffer before it gets rendered onto the main screen.

![Kiero and MinHook Initialization in C++](2.jpg)

### Step 3. Injecting the DLL into the Game
Since the utility compiles into a `.dll` file, it needs to be injected into the running `stormworks.exe` process. You can use any standard DLL Injector for this (e.g., Process Hacker, Cheat Engine, or your own custom launcher).

![DLL Injection Process](3.jpg)

### Step 4. Final Result
Once successfully injected, the game continues running smoothly without any performance loss, while a new standalone custom window appears on your desktop, duplicating the selected camera feed.

![Stormworks Camera in a Separate Window](4.jpg)

---

## 🛠 Compilation Requirements

To successfully build this project from source, you will need the following dependencies:

1. **Kiero Hook (v2)** — A universal tool for finding graphic API function addresses. It is required to locate the pointer for the OpenGL graphics context function in Stormworks.
   
2. **MinHook** — A minimalist x86/x64 API hooking library written in C. It redirects the game's original rendering function call to your custom implementation.

3. **OpenGL (Headers & Extensions)** — Since Stormworks is built on OpenGL, you will need extension headers (such as **GLEW** or **glad**) to manipulate framebuffers and camera textures, as well as linking against `Opengl32.lib`.
---

## 🔍 Important Note on Game Camera Culling

Due to how the Stormworks game engine optimizes rendering, camera textures are dynamically culled (they stop updating) in the following cases:
* If you move too far away from the camera/monitor.
* If you stand extremely close to the monitor or look through walls.
* If you are sitting in a pilot seat and the physical monitor is completely out of your direct line of sight.

**The texture only captures and updates in the separate window when the camera/monitor is actively being rendered on your screen at any distance.**

💡 **Workaround:** To force the game engine to keep the remote camera textures active at all times, place a small local monitor inside your vehicle's cabin or near your seat linked to that camera stream. This forces Stormworks to process the texture, allowing the DLL hook to capture it continuously.

---

## 🍕 Support the Project / Buy me some food

Developing and reversing game code takes a lot of time, effort, and late nights. If this tool helped you or saved your multi-monitor setup, **I really need your support to keep going and buy some food!** Any donation is deeply appreciated.

| **ETH (Ethereum / ERC-20)** | **USDT (ERC-20)** | **TRX (Tron Network)** |
| :---: | :---: | :---: |
| `0xC280Eb895795B1058C8A671eD79aDBFe14c7F71C` | `0xC280Eb895795B1058C8A671eD79aDBFe14c7F71C` | `TU27CurhG86YXTVyeYPWqZYLd9NMfAMWAD` |
| ![ETH QR](eth.jpg) | ![USDT QR](usdt.jpg) | ![TRX QR](trx.jpg) |

*Thank you for supporting independent development!*

---

## 📝 Note on Language
*This project documentation and localized texts were generated with the help of an AI assistant, as I do not speak or write fluent Russian.*
