# convert cli grammar

parser implementation: boost.spirit x3.

```ebnf
paren         = "\(" | "\)" | "(" | ")" ;
single-quoted = "'" , { any-character - "'" } , "'" ;
double-quoted = """ , { "\" , any-character | any-character - """ } , """ ;
word          = non-empty-token-without-space-quote-or-paren ;
arg           = single-quoted | double-quoted | paren | word ;
cmdline       = { arg } ;

convert-command  = "convert" , source , { filter } , output ;
source           = file-spec | generator ;
output           = file-spec ;
file-spec        = [ format-name , ":" ] , path ;
path             = arg ;
generator        = size-option , generator-source ;
generator-source = generator-kind , ":" , generator-value ;
generator-kind   = "xc" | "canvas" | "gradient" | "radial-gradient" ;
filter           = crop-filter | crop-rect-filter | resize-filter
                  | radius-filter | format-filter | pix-fmt-filter
                  | filter-setting | limit-setting | palette-filter
                  | fx-filter ;
size-option      = "-size" , size ;
crop-filter      = "-crop" , [ crop-geometry ] ;
crop-rect-filter = "--crop" , rect ;
resize-filter    = ( "-resize" | "--resize" | "--size" ) , size ;
radius-filter    = ( "-radius" | "--radius" ) , integer ;
format-filter    = ( "-format" | "--format" ) , format-name ;
pix-fmt-filter   = ( "-pix_fmt" | "--pix_fmt" | "-pix_format" | "--pix_format" ) , colour-name ;
filter-setting   = ( "-filter" | "--filter" ) , dither-name ;
limit-setting    = ( "-limit" | "--limit" ) , limit-spec ;
palette-filter   = ( "-palete" | "--palete" | "-palette" | "--palette" ) , palette-spec ;
fx-filter        = "-fx" , expression ;
format-name      = "raw" | "png" | "ppm" | "jpeg" | "ktx" | "ktx2" | "dds"
                  | "pvr" | "zcis" | "ansi" | "utf8" | "rom" | "tga" ;
colour-name      = "rgba8888" | "rgb888" | "rgb565" | "rgb555" | "rgba5551"
                  | "bgr888" | "bgra8888" | "gray8" | "etc1" | "yuv420"
                  | "palette" | "chr_nes" | "rgb" | "rgba" | "rgb24" | "bgr24"
                  | "bgra" | "bgr8888" | "rgb565le" | "rgb555le" | "gray"
                  | "grey" | "pal8" | "yuv420p" ;
dither-name      = "nearest" | "bayer" | "floyd" | "strict" ;
limit-spec       = ( "sort" | "spread" ) , ":" , integer ;
palette-spec     = palette-name , [ ":" , range , { "," , range } ] ;
range            = integer , [ ".." , integer ] ;
size             = integer , ( "x" | "X" ) , integer | integer , ( "%" | "pct" ) ;
crop-geometry    = integer , ( "x" | "X" ) , integer , [ signed-integer , signed-integer ] ;
rect             = integer , "," , integer , "," , integer , "," , integer ;
integer          = digit , { digit } ;
signed-integer   = ( "+" | "-" ) , integer ;
```

## behavior

- `png:-` and `ppm:-` use stdin/stdout with an explicit format.
- an unknown prefix such as `foo:bar.png` is parsed as a normal path.
- every option takes its words after a space or after an `=`; the two are the same thing.
- `-size` is valid only when followed by a generator source, so an option written between the two is an error.
- generator sources are parsed but currently rejected during pipeline construction.
- `-fx` expressions are parsed but currently rejected during pipeline construction.
- `-crop` is the one option whose argument is optional; without geometry it creates a default crop token, which the current c++ pipeline rejects until a default crop operation is defined.
- `-filter` and `-limit` are settings: they wait for the operation that uses them, and a line that ends with one still waiting is an error.
- `-pix_fmt` names the colour stored inside the output, the way ffmpeg's option does; the container still comes from the output path or `-format`.
- naming a colour a container cannot store is an error rather than a silent conversion.
- `raw:out.bin` with a `-pix_fmt` writes the pixels themselves, which is the only way to ask for a colour with no container around it.
- the format and colour names above are generated from the enums, so they are whatever this build can actually do.
