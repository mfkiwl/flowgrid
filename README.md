# FlowGrid

Prototyping the new stack for FlowGrid.
(Old version [here](https://github.com/khiner/flowgrid))

I'm still actively building this.
Currently, I'm basically trying to maximally mash together some wonderful libraries (see [**Stack**](#stack)):

## Build and run

### Mac

Prepare your environment:

```sh
$ git clone --recursive git@github.com:khiner/flowgrid2.git
$ brew install cmake pkgconfig llvm freetype
$ brew link llvm --force
```

Build and run the application (or open the project in your IDE of choice and build - I can only speak for CLion
working):

```sh
$ cmake -B cmake-build-debug
$ cmake --build cmake-build-debug --target FlowGrid -- -j 8
$ cd cmake-build-debug
$ ./FlowGrid
```

TODO: Will probably want to eventually build llvm locally as a submodule, and point to it.
See [TD-Faust](https://github.com/DBraun/TD-Faust/blob/02f35e4343370559c779468413c32179f55c6552/build_macos.sh#L5-L31)
as an example.

## Stack

### Audio

* [Faust](https://github.com/grame-cncm/faust) for DSP
* [libsoundio](https://github.com/andrewrk/libsoundio) for the audio backend

### UI/UX

* [ImGui](https://github.com/ocornut/imgui) for UI
* [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) for file selection
* [ImPlot](https://github.com/epezent/implot) for plotting
* [zep](https://github.com/Rezonality/zep) for code/text editing

### Backend

* [json](https://github.com/nlohmann/json) for state serialization, and for the diff-patching mechanism behind undo/redo
* [ConcurrentQueue](https://github.com/cameron314/concurrentqueue) for the main event queue
* [diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text

### Debugging

* [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

## Development

### Tracing

To enable tracing, set `TRACY_ENABLE` to `ON` in the main project `CMakeLists.txt`.

To build and run the [Tracy](https://github.com/wolfpld/tracy) profiler,

```sh
$ brew install gtk+3 glfw capstone freetype
$ cd lib/tracy/profiler/build/unix
$ make release
$ ./Tracy-release
```

### Updating submodules

#### Non-forked submodules

Most submodules are not forked.
Here is my process for updating to the tip of all the submodule branches:

```sh
$ git submodule update --remote
$ git add .
```

#### Forked submodules

The following modules are [forked by me](https://github.com/khiner?tab=repositories&q=&type=fork), along with the
upstream branch the fork is based on:

* `imgui:docking`
* `implot:master`
* `libsoundio:master`
* `zep:master`

I like to keep my changes rebased on top of the original repo branches.
Here's my process:

```sh
$ cd lib/{library}
$ git pull --rebase upstream {branch} # `upstream` points to the original repo. See list above for the tracked branch
$ ... # Resolve any conflicts & test
$ git push --force
```

A notable exception is my zep fork, which has so many changes that almost no upstream commits will rebase successfully.
The way I handle rebasing against zep is to rebase one commit at a time, using `--strategy-option theirs` (`-Xtheirs`),
and then manually verifying & porting what the merge missed:

```sh
$ cd lib/zep
$ git pull --rebase -Xtheirs upstream {commit_sha}
$ ... # Resolve any conflicts, port any missing changes manually, verify...
$ git push --force
```

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding
any libraries in the `lib` directory with different licenses) must also
be released under the GPL v3 license.

### Why copyleft?

The choice of a copyleft license for this project is not an incidental one.

The audio world has a ton of amazing and fully open-source code, educational resources, libraries, etc.
However, the commercial audio industry is also full of closely guarded secret sauces and protected IP.
This is a necessary strategy for the too-few companies that manage to achieve some level of financial independence in an
industry in which it's notoriously difficult to do so.

Roughly speaking, and all other things being equal, choosing a permissive license that allows for closed-source
commercial usage stands to benefit more end users (musicians, artists, creators) in the short-term, since companies
producing closed-source software could freely put the code right into their products.

However, my experience as both a developer and musician (very much in that order), is basically that musicians are
taken care of pretty darn well, and that audio developers are not very well off at all.
The way I see the situation, it's very easy, and always getting easier, to find the software/hardware you need to make a
track, or a generative audio piece, or whatever your jam is.
I've found it's much harder to find an effective path to creating new audio software.

Ultimately, although this project is first and foremost a creative tool, the intention and spirit is much more about
hacking, learning, education and research than it is about creating end media products.
For these purposes, it's much more important to ensure the information stays open than it is to make the functionality
freely and widely available.
