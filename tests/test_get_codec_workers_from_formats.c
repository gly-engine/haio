#include <stddef.h>
#include <assert.h>

#define RAIO_STUB
#define RAIO_ONLY_PROTO

#include "raio.h"
#include "raio/codec_names.h"


int main()
{
    raio_worker_func_t *workers;
    raio_type_t output;
    {
        assert(GetCodecWorkersFromFormats(RAIO_TYPE_BUFFER, RAIO_TYPE_IMG_PNG, NULL, 0, &output));
        assert(output == RAIO_TYPE_IMG_RGBA8);
    }
    {
        assert(GetCodecWorkersFromFormats(RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_PPM, workers, 10, &output));
        assert(output == RAIO_TYPE_IMG_PPM);
    }
    {
        assert(GetCodecWorkersFromFormats(RAIO_TYPE_IMG_PNG, RAIO_TYPE_IMG_Y4M420, NULL, 0, NULL));
    }
    return 0;
}
