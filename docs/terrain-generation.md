# Terrain generation and `.gmap` format

This document describes the deterministic terrain pipeline implemented by TerrainGenerator. It is intended as a technical reference for reproducing compatible maps in another language.

## Pipeline

| Stage | Input | Output | Purpose |
| --- | --- | --- | --- |
| Map generation | Generator settings and `GENSEED` | `(width + 1) x (height + 1)` control grid and one seed per level | Establishes the shared terrain shape and local-detail streams. |
| Border expansion | Control grid, `GENSEED`, and level settings | Continuous 65-sample borders for every level | Makes adjacent levels meet exactly. |
| Level generation | Four borders, a per-level seed, level settings | A `65 x 65` height field | Adds deterministic local detail inside a level. |
| Optional overrides | A `9 x 9` control patch | Modified local height field | Applies saved manual edits smoothly across the level. |
| Preview | Four nearby samples | A terrain color | Produces the editor's projected terrain view. |

All coordinates are zero-based in the implementation. Map controls have dimensions `width + 1` by `height + 1`; level seeds have dimensions `width` by `height`.

## Deterministic random stream

The entire generator uses one unsigned 32-bit linear congruential generator:

```text
state = (state * 0x08088405 + 1) mod 2^32
random_unit = state / 4294967296.0
```

Use unsigned overflow deliberately. Do not use a host-language general-purpose random API, a signed multiply, or a floating-point seed. Every call consumes exactly one state transition.

| Operation | Returned value | State advance |
| --- | --- | --- |
| `drand()` | `uint32(state) * 2^-32`, in `[0, 1)` | One step |
| `drandi()` | The same new bit pattern interpreted as `int32` | One step |

Python-compatible reference:

```python
class TerrainRandom:
    def __init__(self, seed: int):
        self.state = seed & 0xffffffff

    def drand(self) -> float:
        self.state = (self.state * 0x08088405 + 1) & 0xffffffff
        return self.state / 4294967296.0

    def drandi(self) -> int:
        self.state = (self.state * 0x08088405 + 1) & 0xffffffff
        return self.state if self.state < 0x80000000 else self.state - 0x100000000
```

The order of calls is part of the format contract. A compatible implementation must preserve every loop order and recursion order below.

## Recursive displacement

`midpoint` fills the centre of a diagonal rectangle, then recursively fills its two diagonal children.

```text
midX = (left + right) / 2     // integer division
midY = (top + bottom) / 2
height[midY, midX] = (height[top, left] + height[bottom, right]) / 2
                    + (drand() - 0.5) * 2 * chaos

midpoint(midY, midX, top, left, chaos * chaosScale)
midpoint(bottom, right, midY, midX, chaos * chaosScale)
```

It stops when both dimensions are at most one cell. `quadrant` first fills the horizontal, vertical, and two centre axes with three midpoint passes, then recursively visits quadrants in this fixed order: top-left, top-right, bottom-left, bottom-right.

| Function | Initial chaos | Decay per recursive depth |
| --- | ---: | ---: |
| Map perimeter and map quadrant | `GENHEIGHT` | `GENCHAOS` |
| Shared 64-sample border segments | `LEVHEIGHT` | `LEVCHAOS` |
| Level interior | `LEVHEIGHT * LEVCHAOS` | `LEVCHAOS` |

The calculation intentionally uses `double` until control values are finalized.

## Map controls and per-level seeds

Start the control grid with `GENBASE` in every cell. Create one `TerrainRandom` using `GENSEED`.

If `GENEVENBORDERS` is false, apply the four perimeter midpoint passes in this exact order:

| Pass | Rectangle `(bottom, right, top, left)` |
| --- | --- |
| Top | `(0, width, 0, 0)` |
| Bottom | `(height, width, height, 0)` |
| Left | `(height, 0, 0, 0)` |
| Right | `(height, width, 0, width)` |

Then run `quadrant(height, width, 0, 0)` over the whole map control grid. Finalize every control with:

```text
control = floor(control + 0.0001)
```

The small positive offset keeps a value intended to be integral from rounding down solely because of floating-point representation.

The same random stream then produces a seed for every level. The generation loop is **x-major**, but the stored seed array is row-major:

```text
for x in 0 .. width - 1:
    for y in 0 .. height - 1:
        randomSeeds[y * width + x] = floor(drand() * 2147483647)
```

This distinction matters. Iterating `y` first produces a different serialized `RANDOMSEEDS` block and different level detail.

Example for a `2 x 2` map: generation writes seeds in `(0,0), (0,1), (1,0), (1,1)` order, while serialization writes rows `(0,0), (1,0)` then `(0,1), (1,1)`.

## Shared borders and local levels

Each level has a `65 x 65` sample field. Its corners are the map controls at `(levelX, levelY)` through `(levelX + 1, levelY + 1)`.

Generate two shared dense border buffers before generating any individual level:

| Buffer | Dimensions | Holds |
| --- | --- | --- |
| Horizontal | `(mapHeight + 1) x (mapWidth * 64 + 1)` | Every horizontal level boundary |
| Vertical | `(mapHeight * 64 + 1) x (mapWidth + 1)` | Every vertical level boundary |

Initialize every 64-sample intersection from the control grid. Then run midpoint generation through segments in column-major order:

```text
for x in 0 .. width:
    for y in 0 .. height:
        if x < width:  fill horizontal segment (x, y) to (x + 1, y)
        if y < height: fill vertical segment   (x, y) to (x, y + 1)
```

This uses a new stream initialized from `GENSEED`, shared by all segments. Reinitializing it for every level breaks continuity at the shared borders.

Generate World derives the detail settings before expanding those borders:

```text
LEVHEIGHT = GENHEIGHT * GENCHAOS ^ log2(WIDTH)
LEVCHAOS  = GENCHAOS
```

For the standard `32 x 32` settings, `LEVHEIGHT = 65 * 0.6^5 = 5.0544`. Using `GENHEIGHT` directly for dense borders makes detail displacement about thirteen times too large.

To construct a level:

1. Copy its top and bottom rows from the horizontal buffer.
2. Copy its left and right columns from the vertical buffer.
3. Create a stream from that level's `RANDOMSEEDS` entry.
4. Run `quadrant(64, 64, 0, 0)` with `LEVCHAOS` and initial chaos `LEVHEIGHT * LEVCHAOS`.

Only the interior is randomized at this stage: the four copied edges remain the shared edges.

## Manual height overrides

`HEIGHTS <level-name>` contains a 9 by 9 grid aligned to every eighth sample of a level: sample coordinates `(0, 8, 16, ..., 64)`.

For each override point, calculate its delta from the generated height at that position. Apply a separable triangular falloff over seven samples in each direction:

```text
deltaStep = (override - generatedAtControl) / 8
rowWeight = 8 - abs(row - controlY)
columnWeight = 8 - abs(column - controlX)
addedHeight = deltaStep * rowWeight * columnWeight / 8
```

At the control point the full delta is applied. At the seventh neighboring sample the contribution is one eighth of the delta; it is zero eight samples away. This makes edits meet at control points without producing a hard square boundary.

For a map-level edit, update the matching `HEIGHTMAP` control. Its neighboring levels derive their common corners from the same value. Use a `HEIGHTS` block only when a particular level needs persistent local deformation independent of the shared map controls.

## `.gmap` layout

The text format starts with `GRMAP001`, followed by map settings and structured blocks.

| Field | Meaning |
| --- | --- |
| `WIDTH`, `HEIGHT` | Number of levels, not number of controls. |
| `GENERATED` | Template name used for level filenames. |
| `GENSEED` | Map stream seed; governs map controls and shared borders. |
| `GENBASE`, `GENEVENBORDERS`, `GENHEIGHT`, `GENCHAOS` | Map generation settings. |
| `LEVHEIGHT`, `LEVCHAOS` | Local level-detail settings. |
| `MAPIMG` | Map image path/name. |
| `HEIGHTMAP` ... `HEIGHTMAPEND` | `height + 1` rows of `width + 1` decimal control heights. |
| `RANDOMSEEDS` ... `RANDOMSEEDSEND` | `height` rows of `width` unsigned seeds. |
| `HEIGHTS name` ... `HEIGHTSEND` | Optional 9 by 9 local override grid for one level. |

Minimal example:

```text
GRMAP001
WIDTH 2
HEIGHT 1
GENERATED terrain_ab-01.nw
GENSEED 12345
GENBASE 0
GENEVENBORDERS false
GENHEIGHT 65
GENCHAOS 0.6
LEVHEIGHT 25
LEVCHAOS 0.5
MAPIMG terrain.png

HEIGHTMAP
0,4,0
0,-2,0
HEIGHTMAPEND

RANDOMSEEDS
101,202
RANDOMSEEDSEND
```

## Serialization rules

For byte-stable output, preserve this ordering:

1. Fixed header fields through `MAPIMG`.
2. A blank line, `HEIGHTMAP`, row-major decimal rows, and `HEIGHTMAPEND`.
3. A blank line, `RANDOMSEEDS`, row-major unsigned rows, and `RANDOMSEEDSEND`.
4. A blank line, then optional `HEIGHTS` blocks in document order.

Use comma-separated values with no added spaces. Serialize decimal heights with enough precision to round-trip a `double` (`15` significant digits is used here). Parse integer seeds as unsigned 32-bit values.

## Generated level files

When level-file export is enabled, create one `.nw` file for every generated level name next to the `.gmap`. A level with no local edit is a valid minimal level:

```text
GLEVNW01
```

If a level has a local 9 by 9 override, append its `HEIGHTS` block to that header. The map still owns the shared terrain and seeds; the level file stores only the independent local override.

For a new save, derive `GENERATED` from the chosen map filename. For example, saving a 32 by 32 map as `myworld.gmap` produces `GENERATED myworld_bf-32.nw`, then emits `myworld_aa-01.nw` through `myworld_bf-32.nw`.

## Preview colors

The preview shades a quad from four neighboring heights. First compute their average and select a terrain band:

| Average range | Base RGB |
| --- | --- |
| `< -8` | `(0, 80, 111)` |
| `[-8, -2)` | `(0, 127, 158)` |
| `[-2, 3)` | `(102, 102, 0)` |
| `[3, 25)` | `(0, 224, 0)` |
| `[25, 55)` | `(160, 0, 0)` |
| `>= 55` | `(224, 240, 240)` |

When slope lighting is enabled, calculate:

```text
light = clamp(round(((bottomLeft + bottomRight) - (topLeft + topRight)) * 16), -96, 95)
finalRGB = clamp(baseRGB + light, 0, 255)
```

That is a north/south height difference, not a conventional normalized surface normal. Apply it identically to red, green, and blue.

## Compatibility checklist

| Check | Expected result |
| --- | --- |
| Same settings and `GENSEED` | Identical `HEIGHTMAP` values and `RANDOMSEEDS` values. |
| Neighboring generated levels | Shared 65-sample edge values are identical. |
| Parse then serialize without changes | Same field/block ordering and values. |
| Regenerate | Replaces map controls and level seeds and removes old `HEIGHTS` overrides. |
| A local override | Changes only the named level's generated samples. |
| A shared control edit | Changes the corresponding shared corner for every adjacent level. |
