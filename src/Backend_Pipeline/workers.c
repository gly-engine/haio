#include <stddef.h>

#include "raio.h"
#include "raio/codec_names.h"
#include "raio/workers.h"

/*
BUFFER
|
BUFFER FULL
|
PNG------\
          V
    +--->RGBA8 --+ -------+-------+
    |    |       |        |       |
    |    |      BMP     PPM      YUV
 CROP <--|                        |
                                 Y4M
*/

// faca funcoes privadas de patch find (letra minucusla e statica)
// e tambem faça com que cada add step ou end, ele ja vai procurando o melhor caminho

raio_pipeline_t* PipelineBegin(raio_type_t first_step) {
    // implement
    return NULL;
}

// add step to queue
void PipelineStepAdd(raio_pipeline_t* pipe, raio_type_t next_step) {
    
}

// return 0 if a complete patch find!
int PipelineEnd(raio_pipeline_t* pipe, raio_type_t next_step) {
    return 0;
}

// @todo refact
const raio_worker_t *GetPipelineWorker(raio_type_t from, raio_type_t to) {
    return NULL;
}
