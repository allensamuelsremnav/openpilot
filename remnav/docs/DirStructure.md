## Root Directory
The directory root has projects belonging to different orgs. So "Remnav" folder represents the source code that is being worked on by us. It is likely we will need source code from other repos and so we can use git submodules to track those external dependencies at the root level.

It would also contain other global information like licensing, installation instructions, release information etc.

## Remnav Directory
This holds the various sw components written by us. Each component would either produce a

* Static library for sharing with other components.

* Executable for the platform. This could be windows or linux.

### Component Dir
In general each component will have a 
* src directory is the root of the source code of the component.
* include directory holds the public header files esp for libraries. 
* unit_tests : Using gtest for it.
* tests: integration tests. Generally useful for executables.

Currently there are 2 components that are checked in.

#### Logger Module
This provides structured logging to all other modules. The messages are defined using [capnp proto] (https://capnproto.org/). Using this library the components can write these messages to a common log file. This log file can then be post processed by python to extract meaningful data.

For reader ease, the capnp messges are placed in a single file. Modules can define their own message files and include it in this global file.

#### Audio Module
This generates 4 executables for controlling the speaker and microphone on windows and linux. 
The src directory is further split into lnx and win for platform specific code.

## Docs Directory
This holds component level documentation organized with releases.