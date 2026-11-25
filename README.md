<img width="1254" height="1254" alt="aea662d9-515b-457d-8ece-515f71db396b" src="https://github.com/user-attachments/assets/fcb60a29-9701-424c-a83d-158ae94734a9" />

A portable C11 implementation of 20Q by Robin Burgener using the model from the PSP release. Includes a small inference library and a playable command-line game.

## Build

Requires a C11 compiler, CMake and Ninja.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build
```

## Play

Run from the project directory:

```sh
./build/twentyq --bank en
```

On Windows:

```powershell
.\build\twentyq.exe --bank en
```

Think of an object, choose its category, and answer the questions. Enter a displayed number, answer label, or letter such as `Y`, `N`, or `U`.

Use `undo` to take back an answer, `restart` for a new game, or `quit` to exit. `candidates` shows the current guesses and scores.

| Option | Purpose |
| --- | --- |
| `--bank en` | Choose a language: `en`, `de`, `es`, `fr`, `it`, `jp` |
| `--seed 1` | Set the random seed for repeatable games |
| `--trace` | Show candidate scores, strategy and random usage |
| `--data DIR` | Set the model directory; defaults to `data` |
| `--model FILE` | Load a specific `.q20` file |

English, German, Spanish, French, Italian and Japanese dictionaries are included. Menu prompts are in English. The incomplete Greek bank is not supported.

## Library

The inference core is separate from the terminal interface. Link the `q20` CMake target and include [`q20.h`](include/q20.h).

```cmake
add_subdirectory(path/to/20q)
target_link_libraries(your_app PRIVATE q20)
```

Load a model with `q20_load`, create a session with `q20_create`, then call `q20_advance` to get the next question or guess. Read it with `q20_status` and submit an answer with `q20_submit`. `q20_reset` starts another game.

Models can be shared between sessions. Each session owns its game state. Destroy sessions before unloading their model. See [`src/cli.c`](src/cli.c) for a complete example.

## Model data

The `.q20` files contain the original packed four-bit model, dictionaries, IDs and scoring tables. The English file is about 735 KiB; all six banks total about 1.53 MiB. The engine does not train or modify the model.

To regenerate the containers from the extraction exports:

```sh
python tools/prepare_models.py --source ../output/data --out data
```

The file layout is documented in [`FORMAT.txt`](FORMAT.txt). Implementation notes and source-function mappings are in [`BEHAVIOR.txt`](BEHAVIOR.txt).

## Tests

The comparison tests use Python, Unicorn and the original extraction files. They cover model reads, scoring, question selection and complete game sequences.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The test setup and extraction paths are listed in [`README.txt`](README.txt).
