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
file-spec        = [ format-prefix , ":" ] , path ;
format-prefix    = "png" | "ppm" | "etc1" | "rgb565" | "pvr" | "dds" | "ktx" | "ktx2" | "raw" ;
path             = arg ;
generator        = size-option , generator-source ;
size-option      = "-size" , size ;
generator-source = generator-kind , ":" , generator-value ;
generator-kind   = "xc" | "canvas" | "gradient" | "radial-gradient" ;
filter           = crop-filter | resize-filter | radius-filter | format-filter | fx-filter ;
crop-filter      = "-crop" , [ crop-geometry ]
                  | "--crop" , rect ;
resize-filter    = ( "--size" | "--resize" | "-resize" ) , size ;
radius-filter    = ( "--radius" | "-radius" ) , integer ;
format-filter    = ( "--format" | "-format" ) , format-prefix ;
fx-filter        = "-fx" , expression ;
size             = integer , ( "x" | "X" ) , integer ;
crop-geometry    = integer , ( "x" | "X" ) , integer , [ signed-integer , signed-integer ] ;
rect             = integer , "," , integer , "," , integer , "," , integer ;
integer          = digit , { digit } ;
signed-integer   = ( "+" | "-" ) , integer ;
```

## behavior

- `png:-` and `ppm:-` use stdin/stdout with an explicit format.
- an unknown prefix such as `foo:bar.png` is parsed as a normal path.
- `-size` is valid only when followed by a generator source.
- generator sources are parsed but currently rejected during pipeline construction.
- `-fx` expressions are parsed but currently rejected during pipeline construction.
- `-crop` without geometry creates a default crop token; the current c++ pipeline rejects it until a default crop operation is defined.
- `-crop wxh+x+y` and `--crop x,y,w,h` build rectangle crop steps.
