# FlowGrid

Faust in an ImGui interface backed by a persistent (as in persistent data structures), fully-undoable store.

_Still actively building this. Expect main to be occasionally broken._

My goal with FlowGrid is to create a framework for making artful/(self-)educational/useful interactive audiovisual programs.

Things to play with:

- Change configuration of audio read/write processes.
- Create audio DSP with Faust.
- Inspect the full FlowGrid application state, and its full history.
- Navigate to any point in the project's history lightning fast.
  - See [Application architecture][#application-architecture] for more details.
- Other things to come! Definitely planned:
  - MIDI
  - USB (supporting display of any window in a Push 2 LCD display)
  - HLSL (or another shader language, not settled on one)

## Application architecture

The fundamental aspects of FlowGrid's state management architecture are inspired by [Lager](https://github.com/arximboldi/lager), particularly its value-oriented design and unidirectional data-flow architecture.
Lager, in turn, is inspired by frameworks like [Elm](https://guide.elm-lang.org/architecture) and [Redux](https://redux.js.org/introduction/getting-started).
The spirit of the application architecture is similar to Lager, but much simpler, and with none of its dependencies other than [immer](https://github.com/arximboldi/immer/).

FlowGrid uses a single [persistent map](https://sinusoid.es/immer/containers.html#map) to store the full application state.
Every user action that affects the application state results in the following:

- Put a snapshot of the current application state into history.
  This is only a snapshot conceptually, as only a small amount of data actually needs to be copied to keep a full log of the history, due to the space-efficiency of the [Hash-Array-Mapped Trie (
  ) data structure](https://youtu.be/imrSQ82dYns).
- Generate an `Action` instance containing all the information needed to execute the logical action.
- Pass this action to the `State::Update` function, which computes and returns a new immutable store (an `immer::map` instance) containing the new state after running the action.
- The single canonical application store instance is overwritten with this new resultant store.
  This assignment is thread-safe.
  Due to the immutability of the canonical application store instance, readers will always see a consistent version of either the old state (before the action) or the new state (after the action).

A single instance of a simple nested struct of type `State` wraps around the application store and provides hierarchically organized access, metadata (like its path in the store, or its corresponding ImGui ID), and methods for transforming or rendering the state member.
Each leaf member of `State` is a `Field` or simple collections of `Field`s.
A `Field` is a thin wrapper around its corresponding `Primitive` value in the application store (of type `immer::map<Path, Primitive>`).
A `Primitive` is defined as:

```cpp
using Primitive = std::variant<bool, unsigned int, int, float, string>;
```

`Field`s also provide state metadata, conversion & rendering methods, and behave syntactically like the `Primitive`s they wrap.

## Application docs

### Project files

FlowGrid supports three project formats.
When saving a project, you can select any of these formats using the filter dropdown in the lower-right of the file dialog.
Each type of FlowGrid project file is saved as plain JSON.

- `.fgs`: _FlowGrid**State**_
  - The full application state.
    An `.fgs` file contains a JSON blob with all the information needed to get back to the saved project state.
    Loading a `.fgs` project file will completely replace the application state with its own.
  - As a special case, the project file `./flowgrid/empty.fgs` (relative to the project build folder) is used internally to load projects.
    This `empty.fgs` file is used internally to implement the `open_empty_project` action, which can be triggered via the `File->New project` menu item, or with `Cmd+n`.
    FlowGrid (over-)writes this file every launch, after initializing to empty-project values (and, currently, rendering two frames to let ImGui fully establish its context).
    This approach provides a pretty strong guarantee that loading a new project will always produce the same, valid empty-project state.
- `.fga`: _FlowGrid**Actions**_
  - FlowGrid can also save and load projects as a list of _action gestures_.
    This format stores an ordered record of _every action_ that affected the app state up to the time it was saved.
    More accurately, an `.fga` file is a list _of lists_ of (action, timestamp) pairs.
    Each top-level list represents a logical _gesture_, composed of a list of actions, along with the absolute time they occurred.
    Each action item contains all the information needed to carry out its effect on the application state.
    In other words, each list of actions in an `.fga` file tells you, in application-domain semantics, what _happened_.
  - **Gesture compression:** Actions within each gesture are compressed down to a potentially smaller set of actions.
    This compression is done in a way that retains the same application-state effects, while also keeping the same application-domain semantics.

## Clean/Build/Run

This project uses LLVM IR to JIT-compile Faust code.
To simplify, make things more predictable, and reduce bloat, we use the LLVM ecosystem as much as possible.

We use `clang++/clang` to compile, and LLVM's `lld` for linking.
Even if it's not strictly required, we generally aim to use the newest available LLVM point release (`x.0.1` and beyond).
If the project does not build correctly for you, please make sure your `clang`, `lld`, and `clang-config` point to the newest available point-release of LLVM.
If that doesn't work, try the latest release in the previous LLVM major version.

**VSCode:** The `.vscode/launch.json` config has debug/release launch profiles (todo actually add release profile).

### Mac

- **Install system requirements:**

```shell
$ git clone --recursive git@github.com:khiner/flowgrid.git
$ brew install cmake pkgconfig llvm freetype
$ brew link llvm --force
```

All scripts can be run from anywhere, but to the root repo directory (clean/build).

- **Clean:**
  - Clean up everything: `./script/Clean`
  - Clean debug build only: `./script/Clean -d [--debug]`
  - Clean release build only: `./script/Clean -r [--release]`
- **Build:**
  - Debug build (default): `./script/Build`
  - Release build: `./script/Build -r [--release]`

Debug build is generated in the `./build` directory relative to project (repo) root.
Release build is generated in `./build-release`.

To run the freshly built application:

```sh
# The application assumes it's being run from the build directory when locating its resource files (e.g. font files).
$ cd build # or build-release
$ ./FlowGrid # application must be run from a directory above root. todo run from anywhere
```

If the build/run doesn't work for you, please [file an issue](https://github.com/khiner/flowgrid/issues/new), providing your environment and any other relevant details, and I will try and repro/fix!

## Stack

### Audio

- [Faust](https://github.com/grame-cncm/faust) for DSP
- [miniaudio](https://github.com/mackron/miniaudio) for the audio backend
- ~~[r8brain-free-src](https://github.com/avaneev/r8brain-free-src/)~~ (_not actually used atm_): audio resampling, currently only used when monitoring an audio input stream with a sample rate different from the output stream

### UI/UX

- [ImGui](https://github.com/ocornut/imgui): UI & interactions
- [ImPlot](https://github.com/epezent/implot): plotting
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog): file selection
- [zep](https://github.com/Rezonality/zep): code/text editing
- [ImGui memory_editor](https://github.com/ocornut/imgui_club): viewing/editing memory directly

### Backend

- [immer](https://github.com/arximboldi/immer): persistent data structures for the main application state store
  - Used to quickly create, store, and restore persistent state snapshot
    (used for undo/redo, and for debugging/inspection/monitoring)
- [json](https://github.com/nlohmann/json): state serialization
- [ConcurrentQueue](https://github.com/cameron314/concurrentqueue): the main action queue
  - Actions are _processed_ synchronously on the UI thread, but any thread can submit actions to the queue, via the global `q` method.
- ~~[diff-match-patch-cpp-stl](https://github.com/leutloff/diff-match-patch-cpp-stl) for diff-patching on unstructured
  text~~
  - Was using this to handle ImGui `.ini` settings string diffs, but those are now deserialized into the structured state.
    I'll likely be using this again at some point for generic handling of actions involving long text strings.

### C++ extensions

For C++20 features only partially/experimentally supported in Clang 16:

- [range-v3](https://github.com/ericniebler/range-v3)
- [fmt](https://github.com/fmtlib/fmt)

### Debugging

- [Tracy](https://github.com/wolfpld/tracy) for real-time profiling

## Development

I try and keep all dependencies up to date.
LLVM version 15+ is required to build.

### Formatting

FlowGrid uses `clang-format` for code formatting.

Use the provided [pre-commit hook](.githooks/pre-commit) to automatically format staged files for each commit.
**Please enable this hook before contributing** by running:

```sh
git config --local core.hooksPath .githooks/
```

In addition, the `Format` script formats every source Cxx file.

### Tracing

To enable tracing, set `TRACY_ENABLE` to `ON` in the main project `CMakeLists.txt`.
todo only build/include tracy in a new build type, built in
todo create a third build type for

To build and run the [Tracy](https://github.com/wolfpld/tracy) profiler, run:

```sh
$ brew install gtk+3 glfw capstone freetype
$ cd lib/tracy/profiler/build/unix
$ make release
$ ./Tracy-release
```

### Updating submodules

All submodules are in the `lib` directory.

#### Non-forked submodules

Most submodules are not forked.
Here is my process for updating to the tip of all the submodule branches:

```sh
$ git submodule update --remote
$ git add .
$ git cm -m "Update libs"
```

#### Forked submodules

The following modules are [forked by me](https://github.com/khiner?tab=repositories&q=&type=fork), along with the upstream branch the fork is based on:

- `imgui:docking`
- `implot:master`
- `zep:master`
- `imgui_club:master`

I keep my changes rebased on top of the original repo branches.
Here's my process:

```sh
$ cd lib/{library}
$ git pull --rebase upstream {branch} # `upstream` points to the original repo. See list above for the tracked branch
$ ... # Resolve any conflicts & test
$ git push --force
```

A notable exception is my zep fork, which has so many changes that almost no upstream commits will rebase successfully.
The way I handle rebasing against zep is to rebase one commit at a time, using `--strategy-option theirs` (`-Xtheirs`), manually resolving any rebase conflicts:

```sh
$ cd lib/zep
$ git pull --rebase -Xtheirs upstream {commit_sha}
$ ... # Resolve any conflicts, port any missing changes manually, verify...
$ git push --force
```

### Current development goals

So far, I'm basically trying to mash together some great libraries.
Here are some of my current development thoughts/goals, roughly broken up into abstract/concrete:

#### Abstract development goals

- **Keep it as simple as I possibly can.**
- Focus on making it fun to use _and create_ the application.
- Spend more time up front getting the foundation right (simple, transparent, flexible, powerful).
- Keep (re-)build times low.
- At this early stage, my main application goal is to _facilitate the development of the app_.
  - Invest early in things like adding debugging capabilities.
  - Make the application state and context transparent and easily modifiable.
  - Provide performance & debug metrics in real-time.
- Let myself optimize.
  In computers, fast things are good and always more fun than slow things.
- Prioritize learning over development velocity.
  Dig into, and take ownership over, low-level concerns where appropriate.
  Feed curiosity.
  Bias towards reinventing wheels over accepting bloated/overly-complex dependencies that do too much.

#### Concrete development goals

- Store the source-of-truth application state in a single `struct` with global read access.
- Perform all actions that affect the application state in one place.
- Provide global read access to all application runtime state.
- Make _everything_ undo/redo-able.
- As much as possible, make the UI a pure function of the application state.

## License

This software is distributed under the [GPL v3 License](./LICENSE).

GPL v3 is a strong copyleft license, which basically means any copy or modification of the code in this repo (excluding any libraries in the `lib` directory with different licenses) must also be released under the GPL v3 license.

### Why copyleft?

The audio world has lots of high quality open-source code, educational resources, libraries, and other software resources.
However, the commercial audio software industry is also full of protected IP.
This is a necessary strategy for the too-few companies that manage to achieve some level of financial independence in an industry where that's a hard thing to do.

Choosing a permissive license allowing for closed-source commercial usage may stand to benefit more end users (musicians, artists, creators) in the short-term, since companies producing closed-source software could freely put the code right directly their products without violating copyright.

I usually find it pretty easy to find the music software or hardware I need as a _music producer_.
As a _developer_, I've found it much harder to find the resources, tooling, and methodologies I need to easily create effective new audio software.

Although this project is first and foremost a creative tool, the intention and spirit is much more about hacking, learning, educating and researching than it is about producing end media products.
For these purposes, keeping the information open is more important than making the functionality freely and widely available.
