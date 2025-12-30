#include <stddef.h>
#include <stdlib.h>

#include "img.h"

raio_pipeline_t *BackendPipelineFromUrl(char *const url)
{
    raio_pipeline_t *res = NULL;
    raio_pipeline_t* root = NULL;
    raio_step_t* steps[2] = {0};

    char* ext = GetExtensionFromString(url);
    raio_type_t format = GetFormatFromExtension(ext);

    do {
        if (format == RAIO_TYPE_NULL) {
            break;
        }

        const raio_worker_t* workers[] = {
            GetPipelineWorker(RAIO_TYPE_URL_FILE, RAIO_TYPE_BUFFER),
            GetPipelineWorker(RAIO_TYPE_BUFFER, format)
        };

        if (!workers[0] || !workers[1]) {
            break;
        }

        root = calloc(1, sizeof(raio_pipeline_t));
        steps[0] = calloc(1, sizeof(raio_worker_t));
        steps[1] = calloc(1, sizeof(raio_worker_t));
        
        if (!root || !steps[0] || !steps[1]) {
            break;
        }

        res = root;
        steps[0]->worker = workers[0];
        steps[1]->worker = workers[1];
        root->head = steps[0];
        root->tail = steps[1];
        root->head->next = steps[1];
    }
    while(0);

    if (root) free(root);
    if (steps[0]) free(steps[0]);
    if (steps[1]) free(steps[1]);

    return res;
}
