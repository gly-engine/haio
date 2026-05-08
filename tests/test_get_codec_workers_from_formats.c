#include <stddef.h>
#include <assert.h>

#include "haio.h"
#include "haio/functions.h"

#define HAIO_STUB
#define HAIO_ONLY_PROTO

#include "haio/codec_names.h"

int main()
{
    haio_worker_func_t workers[10];
    haio_type_t output;
    {
        output = HAIO_TYPE_NULL;
        assert(GetCodecWorkersFromFormats(HAIO_TYPE_BUFFER, HAIO_TYPE_IMG_PNG, NULL, 0, &output));
        assert(output == HAIO_TYPE_IMG_RGBA8);
    }
    {
        output = HAIO_TYPE_NULL;
        assert(GetCodecWorkersFromFormats(HAIO_TYPE_IMG_RGBA8, HAIO_TYPE_IMG_PPM, workers, 10, &output));
        assert(output == HAIO_TYPE_IMG_PPM);
    }
    {
        output = HAIO_TYPE_NULL;
        assert(GetCodecWorkersFromFormats(HAIO_TYPE_BUFFER, HAIO_TYPE_IMG_Y4M420, NULL, 0, &output));
        assert(output == HAIO_TYPE_IMG_Y4M420);
    }
    return 0;
}
