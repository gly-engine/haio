# ZCIS

**Zedia Container Image Stream** is an image transport format based on `.ar` _(Unix files)_, designed for progressive image streaming over slow connections and low-performance hardware.

- **Single-request multiple images:** stream several images at once
- **Incremental rendering:** resolution improves as data blocks arrive
- **Easy to mount:** Use the terminal and standard Unix tools.
- **Easy to debug:** extract using common tools and open with image viewer!<br>_(eg. 7zip, Keka, Your Linux Archive Manage...)_
- **Flexible:** use PNG, JPEG, or any image format you like.<br/>_(eg. raw yuyv compression lz4 or whatever.)_

## File identifier

### Header

The header file contains exactly the text `000000000000.txt` and should be the first one added to the Unix files. Inside the header, each `\r\n` contains the address of an image to be cut in decimal format and separated by unique space. Padding is not necessary and is considered base 10, but it can be useful to make it more human-readable.

```
00 00 64 64
65 00 64 64
00 65 64 64
65 65 64 64
```

### Image chunks

Every character in the filename, except for the extension, must be a valid Base62 ASCII character.
The total length of the filename must not exceed 16 characters, in accordance with the Unix .ar specification, and follows ZCIS naming conventions.

```
OTXXYYWWHH.EXT
```

| char | Attribute | description |
| :--: | :-------: | :---------- |
| `0`  | `ORDER`   | ignored, it only serves to facilitate `ar rc $(ls | sort)`
| `T`  | `TYPE`    | command to be executed during image decoding [see more](#commands)
| `XX` | `POSX`    | offset x in px
| `HH` | `POSY`    | offset y in px
| `WW` | `WIDTH`   | width in px (can be stretched to the original image)
| `HH` | `HEIGHT`  | height in px (can be stretched to the original image)

- **NOTE:** that the `ORDER` attribute limits you to 62 files, but this is just a convenience for debugging; filenames can be repeated without issues.

#### Base 62

Base 62 is used to convert bits to ASCII text, considering 0-9, A-Z, and A-Z respectively. The following is a simple JavaScript encoding example. 

```js
const toBase62 = n => n ? toBase62(Math.floor(n/62)) + "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"[n%62] : "0";
```

```js
console.log(toBase62(300));
```

- **NOTE:** Base 62 ranges from 0 to 3843, which is sufficient for 4K images (48040x2160). Larger images are not supported, but you can reach 8K by offsetting a second image.

#### Commands

Image names may specify `WIDTH` and `HEIGHT` independently. When the target dimensions differ from the image dimensions, the image is scaled using **nearest-neighbor (pixel replication)**, preserving each source pixel's color without interpolation.

| Blit | Commit | Description |
| :--: | :----: | :---------- |
| a    | A      | `⠛` Solid tint
| b    | B      | `⠃` Vertical interlaced tint
| c    | C      | `⠉` Horizontal interlaced tint
| d    | D      | `⠁` Stippled tint
| e    | E      | Erase Masking

- **Erase Masking:** Is a `solid tint` of the alpha channel that permanently applies transparency. uses a 1-bit monochrome PCX bitmap, where `1` represents opaque and `0` represents transparent.

## Examples

### An YUV with alpha channel image

| file | content | description |
| :--- | :-----: | :------------ 
| `0000000000.txt` | `0 0 128 128` | delimitation of the image within the container
| `1e00002424.pcx` | | transparency mask
| `2B00002424.y4m` | | YUV image to be filled and displayed

### An RGB progressive stream Image

| file | content | description |
| :--- | :-----: | :------------ 
| `0000000000.txt` | `0 0 128 128` | delimitation of the image within the container
| `1e00002424.pcx` | | 128x128 transparency mask
| `2A00002424.ppm` | | 64x64 RGB image that will be stretched
| `3B00002424.ppm` | | 64x64 RGB image with the complementary pixels to form the full resolution

## Implementation notes

_These notes describe the current c++ encoder (`library/codecs/zcis.cpp`). They are not
part of the standard above; where the two disagree, the standard is what needs a decision._

### What `haio convert input.png output.zcis` emits today

Two layers split by row parity, both scaled back up to the full canvas on display:

| member | stored | target | content |
| :--- | :---: | :---: | :--- |
| `000000000000.txt` | | | `0 0 <width> <height>\r\n` |
| `1A0000WWHH.ppm` | `W x H/2` | `W x H` | the even rows |
| `2B0000WWHH.ppm` | `W x H/2` | `W x H` | the odd rows |

Interleaving the two layers rebuilds the source exactly, so the split costs nothing:
`W x H/2` plus `W x H/2` is the whole image. The first layer alone is a valid preview at
half the bytes, line-doubled vertically. An image one pixel tall emits the base layer only.

The same split holds for a future `4:2:0` payload: each layer keeps `W/2 x H/4` chroma, so
both halves stay decodable on their own and together they restore the original chroma plane
untouched. That needs `height` to be a multiple of 4.

### Open items

- **`ar` truncates the index member.** `000000000000.txt` fills the 16 byte name field with
  no room for the `/` that terminates the sibling names, and GNU `ar` reports it as
  `000000000000.tx`. 7-Zip and libarchive read it correctly. Fixing it means either a 15
  character index name or dropping `/` from the image members — a change to the standard.
- **Command letter versus glyph.** The row split above is written as `B`, but the table
  marks `B` with `⠃`, a vertical bar, which reads as alternating columns; alternating rows
  is `⠉`, the `C` glyph. Either the glyph or the letter needs to move.
  `baseCommand` and `refineCommand` in `library/codecs/zcis.cpp` are the single place to change.
- **Commands `c`, `d` and `e` are not implemented**, so there is no erase masking and no
  stippled or horizontal refinement yet. Only `A` and `B` are emitted.
- **`POSX` and `POSY` are always zero** — no tiling, one image per container.
- **No decoder.** Nothing in this repository reads a `.zcis` back; `haio convert x.zcis y.png`
  fails with `unsupported decode format: zcis`.
- Two details in the standard above still read as typos: the `POSY` row of the filename table
  is labelled `HH` instead of `YY`, and the examples use a ten zero index name while the
  Header section and the encoder use twelve.
