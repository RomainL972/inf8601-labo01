#include <cstdio>
#include <memory>
#include <utility>

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

namespace {
// (image, valeur du canal rouge à ajouter). La valeur est calculée à l'étage de
// chargement -- seul étage garanti serial_in_order -- et transportée jusqu'à
// l'étage add_pixel, plutôt que mutée dans un état partagé par ce dernier :
// ça permet à add_pixel de tourner en mode parallel sans data race, tout en
// produisant exactement la même séquence de valeurs (3, 6, 9, ... mod 256)
// que l'ancienne version, puisque l'ordre de chargement est identique à
// l'ancien ordre d'appel de add_pixel.
using loaded_image_t = std::pair<image_t*, unsigned char>;
}  // namespace

int pipeline_tbb(image_dir_t* image_dir) {
    // TBB stores/invokes filter bodies through a const-qualified call, so a
    // `mutable` lambda capturing `next_red` by value won't compile here --
    // same reason the original code mutated through a captured shared_ptr
    // instead of a plain captured value. Same idiom, just relocated here.
    auto next_red = std::make_shared<unsigned char>(0);

    tbb::parallel_pipeline(
        num_tokens(),
        tbb::make_filter<void, loaded_image_t>(
            tbb::filter_mode::serial_in_order,
            [image_dir, next_red](tbb::flow_control& fc) -> loaded_image_t {
                image_t* image = image_dir_load_next(image_dir);
                if (image == NULL) {
                    fc.stop();
                    return {nullptr, 0};
                }
                *next_red = (*next_red + 3) % 256;
                return {image, *next_red};
            }) &
            tbb::make_filter<loaded_image_t, loaded_image_t>(
                tbb::filter_mode::parallel,
                [](loaded_image_t in) -> loaded_image_t {
                    image_t* new_image = filter_scale_up(in.first, 3);
                    image_destroy(in.first);
                    return {new_image, in.second};
                }) &
            tbb::make_filter<loaded_image_t, image_t*>(
                tbb::filter_mode::parallel,
                [](loaded_image_t in) -> image_t* {
                    pixel_t pixel{.bytes = {0, 0, 0, 0}};
                    pixel.bytes[0] = in.second;
                    image_t* new_image = filter_add_pixel(in.first, &pixel);
                    image_destroy(in.first);
                    return new_image;
                }) &
            tbb::make_filter<image_t*, image_t*>(tbb::filter_mode::parallel,
                                                  [](image_t* image) -> image_t* {
                                                      image_t* new_image = filter_vertical_flip(image);
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
