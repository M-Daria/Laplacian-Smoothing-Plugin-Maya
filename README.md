# Laplacian smoothing plug-in for Maya
Laplacian smoothing is an algorithm to smooth a polygonal mesh. For each vertex in a mesh, a new position is chosen based on local information (such as the position of neighbors) and the vertex is moved there.
## Getting Started
### Prerequisites
What things you need to install the plug-in:
* Autodesk Maya
* Maya Developer Toolkit

All the installation instructions are available on the [link](https://help.autodesk.com/view/MAYAUL/2020/ENU/?guid=Maya_SDK_MERGED_Setting_up_your_build_Windows_environment_64_bit_html). Before starting, you need also add the <b>DEVKIT_LOCATION</b> environment variable that points to your Maya devkit installation directory:

    set DEVKIT_LOCATION=C:\Users\<Username>\devkitBase\
### Running
Use CMake and the appropriate generator to build a project for your code:
1. When building on Windows

        cmake -S. -Bbuild -G "Visual Studio 16 2019"
2. When building using Xcode on MacOS

        cmake -S. -Bbuild -G Xcode
3. When building using a makefile on Linux or MacOS

        cmake -S. -Bbuild -G "Unix Makefiles"
Once you have built your project successfully, use CMake again to build your plug-in:

    cmake --build build
You can now load your plug-in into Maya using the Plug-in Manager, which is accessed from <b>Window > Settings/Preferences > Plug-in Manager</b> from the Maya menu.
<br><br> For example, to load the Windows laplacianSmoothing plug-in, navigate to <b>laplacianSmoothing\build\Debug\ </b> and select <b>laplacianSmoothing.mll</b>.
<br><br> Once loaded, select the object and run from the Maya command window:

    deformer -type laplacianSmoothing
Then select the laplacianSmoothing input and change the <i>Iterations</i> value in the channel box.
