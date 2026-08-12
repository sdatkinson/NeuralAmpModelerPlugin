
# Neural Amp Modeler Plug-in

## Building

Commands are for bash/zsh and apply to Windows (Git Bash) or macOS (Terminal) builds.

If you didn't already clone the repo with: 

    git clone --recurse-submodules https://github.com/sdatkinson/NeuralAmpModelerPlugin

Then you will need to grab the submodules:

    git submodule init
    git submodule update

You will need to install the VST3 SDK for iPlugin2:

    cd iPlug2/Dependencies/IPlug/VST3_SDK/ &&        \
    curl -L https://www.steinberg.net/vst3sdk -O &&  \
    unzip vst3sdk &&                                 \
    mv VST_SDK/vst3sdk/* .

Then build for your platform:

    cd ../../../../NeuralAmpModeler/scripts/

Run one of the makedist-xxx scripts. Windows users should open an 
`x64 Native Tools Command Prompt for VS` command window before running
`makedist-win.bat`

_Note: the build scripts take two arguments, [demo|full] and [zip|installer].
I have only tested the above instructions using `makedist-win.bat demo zip`._

