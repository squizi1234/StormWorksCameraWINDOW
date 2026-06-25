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

Due to Stormworks engine optimization, the camera texture inside the separate window will only update if the in-game monitor is within your character's field of view (in front of you). 

Here is exactly how it behaves:
* **Distance Limit:** It works perfectly at any close range or at a distance **up to 700 meters**, as long as you are looking in its direction. Beyond 700 meters, it stops updating.
* **Through Walls / Obstacles:** It works perfectly fine even if you look at it **through walls or other textures**, as long as the monitor is physically in front of your view.
* **In a Vehicle / Pilot Seat:** If you are driving and the monitor is placed behind you or out of your current front view, it will stop updating. It **must be in front of you and facing your direction**, even if obscured by cockpit geometry.

💡 **Workaround:** If you need to stream a remote camera that is too far away or behind you, place a small secondary monitor directly in front of your pilot seat and link it to that camera. As long as this local screen is in front of you, the engine renders the texture, and the DLL hook will capture it smoothly.

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
