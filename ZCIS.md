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

| Blit | Commit | Description |
| :--: | :----: | :---------: |
| a    | A      | Lines: even, duplicate to odd
| b    | B      | Lines: odd, duplicate to even
| c    | C      | Lines: even, no duplication
| d    | D      | Lines: odd, no duplication
| e    | E      | Columns: even, duplicate to odd
| f    | F      | Columns: odd, duplicate to even
| g    | G      | Columns: even, no duplication
| h    | H      | Columns: odd, no duplication
| z    | Z      | Full image (entire chunk visible)
