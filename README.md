# Lander-Game-CS134
3D game inspired by the retro game Lander in openFrameworks <br>
Created for (and using starter code from) CS134 Computer Game Design with Kevin Smith

## Requirements
- openFrameworks (version at time of creation is 0.12.1)
- Visual Studio 2022 (Windows) or other IDE that supports openFrameworks

## Building
1. Download and install openFrameworks
2. Clone this repository into the /myApps/ folder
3. Extract the data folder from this [Google Drive](https://drive.google.com/file/d/1i6vbQ1bZzP58wG8sfkXOYH3Rxn8BOLLb/view?usp=sharing)
4. Copy the data folder directly into the /bin/ directory. If there is not one, make it in the project root folder
5. Open CS134FinalProject.sln
6. Build and run in Visual Studio
7. Run in Release Mode; 2 Million vertex landscape takes 10 or more minutes in Debug Mode

## Playing
1. Extract the .exe file and /data/ folder
2. Run the .exe file

## Controls and Gameplay
Up and Down arrows - Move Forward/Backward<br>
Left and Right arrows or A/D - Strafe Left/Right<br>
W/S - Thrust Up/Down<br>
Q/E - Rotate Yaw Left/Right<br>
2-8 - Various camera views<br>
L - show Octree leaf nodes<br>
O - show Octree levels

## Contributions
Ethan Wen - Terrain modeling, oF lighting and camera systems, particle systems, audio implementation<br>
Parhuam Jalalian - Ship modeling, physics system, collision detection, landing detection logic
