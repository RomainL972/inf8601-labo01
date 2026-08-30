#include <cstdio>
#include <memory>

#include <tbb/parallel_pipeline.h>

#include <unistd.h>

#include "filter.h"
#include "pipeline.h"

// Nombre de jetons en circulation dans le pipeline TBB, calculé à partir du nombre de coeurs
// logiques de la machine plutot qu'une constante fixe. ×2 (comme l'ancienne version legacy
// de ce laboratoire) pour garder le pipeline saturé même quand une étage (ex.: l'écriture PNG)
// bloque le temps d'un appel système.
static unsigned num_tokens() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? static_cast<unsigned>(n) * 2 : 2;
}

int pipeline_tbb(image_dir_t* image_dir) {
    tbb::parallel_pipeline(
        num_tokens(),
        tbb::make_filter<void, image_t*>(tbb::filter_mode::serial_in_order,
                                          [image_dir](tbb::flow_control& fc) -> image_t* {
                                              image_t* image = image_dir_load_next(image_dir);
                                              if (image == NULL) {
                                                  fc.stop();
                                              }
                                              return image;
                                          }) &
            tbb::make_filter<image_t*, image_t*>(tbb::filter_mode::parallel,
                                                  [](image_t* image) -> image_t* {
                                                      image_t* new_image = filter_scale_up(image, 3);
                                                      image_destroy(image);
                                                      return new_image;
                                                  }) &
            tbb::make_filter<image_t*, image_t*>(tbb::filter_mode::parallel,
                                                  [](image_t* image) -> image_t* {
                                                      image_t* new_image = filter_horizontal_flip(image);
                                                      image_destroy(image);
                                                      return new_image;
                                                  }) &
            tbb::make_filter<image_t*, image_t*>(tbb::filter_mode::parallel,
                                                  [](image_t* image) -> image_t* {
                                                      image_t* new_image = filter_gaussian_blur(image);
                                                      image_destroy(image);
                                                      return new_image;
                                                  }) &
            // parallel, not serial_in_order: image_dir_save names its output file from
            // image->id (set once at load time, propagated by every filter), never from
            // arrival order, so concurrent saves are safe. See the lab's legacy TBB solution
            // (~/git/inf8601/labo-1/source/pipeline-tbb.cpp, SaveFilter) -- pinning the PNG
            // encode+write to one thread capped throughput even with every other filter
            // already parallel.
            tbb::make_filter<image_t*, void>(tbb::filter_mode::parallel,
                                              [image_dir](image_t* image) {
                                                  image_dir_save(image_dir, image);
                                                  printf(".");
                                                  fflush(stdout);
                                                  image_destroy(image);
                                              }));

    printf("\n");
    return 0;
}
