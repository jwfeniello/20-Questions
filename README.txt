20Q
===
A portable C11 implementation of 20Q using the exact model recovered from the
PSP release. Includes an inference library and a playable command-line game.
Runs offline without an emulator.

BUILD AND PLAY
From the project directory, with a C11 compiler, CMake and Ninja installed:

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build

Windows (PowerShell):
.\build\twentyq.exe --bank en

Linux/macOS:
./build/twentyq --bank en

Keep data/ alongside the application, or pass --data DIR to locate it elsewhere.

Think of an object, choose its category, and answer the questions. Enter a
displayed number, answer label, or letter such as Y, N or U.
Commands: restart, quit, candidates, undo.

Options:
  --bank en       Language: en, de, es, fr, it, jp
  --seed 1        Random seed for repeatable games
  --trace         Show strategy, scores and random usage
  --data DIR      Model directory (default: data)
  --model FILE    Load a specific .q20 file

English, German, Spanish, French, Italian and Japanese dictionaries are included.
Menu prompts are English. The incomplete Greek bank is not supported.

LIBRARY
Link the q20 CMake target and include include/q20.h. Load a model with q20_load,
create a session with q20_create, then call q20_advance, q20_status and q20_submit
to play. q20_reset starts another game. Destroy sessions before unloading their
model. See src/cli.c for a complete example.

MODEL CONVERSION AND TESTS
The included data/ files are ready to use. Regenerating them requires the separate
extraction exports:

python tools/prepare_models.py --source ../output/data --out data

Comparison tests also need Python, Unicorn and the original extraction files:
  ../output/data/source/decrypted_eboot.bin
  ../output/data/ (the remaining extraction exports)
  ../output/deliverables_manifest.json

Install Unicorn in the Python environment used by CMake. The test scripts also
look in ../tools/python for the existing extraction setup. These dependencies
are only needed for tests, not for building or playing the game.

To run the tests, create an empty reports directory for generated results, then:
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

PROJECT CONTENTS
README.md                 GitHub overview and usage
CMakeLists.txt            Build configuration
src/, include/            C11 library and terminal application
data/                     Six model containers and their manifest
tests/                    Comparison tests, fixtures and CLI checks
tools/prepare_models.py   Model converter
FORMAT.txt                Model container layout
BEHAVIOR.txt              Implementation notes
