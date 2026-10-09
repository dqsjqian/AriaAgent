# Changelog

## 0.2.0

- Select the published Aria 3.2.0 and Mira 1.1.1 dependencies.
- Move the Linux Qt CI route to GCC 14.
- Add one configure/build/test entry point with explicit platform selection.
- Validate typed CMake definitions, configuration, compilers, source/SDK locations
  and cached build identity before fetching or configuring.
- Apply selected macOS architectures and Visual Studio generator platforms to
  the actual application and test projects.
