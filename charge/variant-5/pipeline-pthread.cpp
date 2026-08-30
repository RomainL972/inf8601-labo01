#include <atomic>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "filter.h"
#include "log.h"
#include "pipeline.h"
#include "queue.h"

#define QUEUE_SIZE 64
#define MAX_STAGE_THREADS 32
#define STAGE_THREAD_SHARE 0.25f

typedef struct stage_ctx {
    image_dir_t* image_dir;
    queue_t* queue_in;
    queue_t* queue_out;
    std::atomic_int* remaining; /* shared by every worker in this stage's pool; NULL for load/save */
    int fanout;            /* number of NULLs to forward once the whole pool has drained */
} stage_ctx_t;

static void* stage_load(void* arg) {
    stage_ctx_t* ctx = static_cast<stage_ctx_t*>(arg);

    while (1) {
        image_t* image = image_dir_load_next(ctx->image_dir);
        if (image == NULL) {
            break;
        }
        queue_push(ctx->queue_out, image);
    }

    for (int i = 0; i < ctx->fanout; i++) {
        queue_push(ctx->queue_out, NULL);
    }

    return NULL;
}

static void* stage_a(void* arg) {
    stage_ctx_t* ctx = static_cast<stage_ctx_t*>(arg);

    while (1) {
        image_t* image = static_cast<image_t*>(queue_pop(ctx->queue_in));
        if (image == NULL) {
            break;
        }

        image_t* new_image = filter_scale_up(image, 3);
        image_destroy(image);
        queue_push(ctx->queue_out, new_image);
    }

    if (ctx->remaining->fetch_sub(1) == 1) {
        for (int i = 0; i < ctx->fanout; i++) {
            queue_push(ctx->queue_out, NULL);
        }
    }

    return NULL;
}

static void* stage_b(void* arg) {
    stage_ctx_t* ctx = static_cast<stage_ctx_t*>(arg);

    while (1) {
        image_t* image = static_cast<image_t*>(queue_pop(ctx->queue_in));
        if (image == NULL) {
            break;
        }

        image_t* new_image = filter_horizontal_flip(image);
        image_destroy(image);
        queue_push(ctx->queue_out, new_image);
    }

    if (ctx->remaining->fetch_sub(1) == 1) {
        for (int i = 0; i < ctx->fanout; i++) {
            queue_push(ctx->queue_out, NULL);
        }
    }

    return NULL;
}

static void* stage_c(void* arg) {
    stage_ctx_t* ctx = static_cast<stage_ctx_t*>(arg);

    while (1) {
        image_t* image = static_cast<image_t*>(queue_pop(ctx->queue_in));
        if (image == NULL) {
            break;
        }

        image_t* new_image = filter_gaussian_blur(image);
        image_destroy(image);
        queue_push(ctx->queue_out, new_image);
    }

    if (ctx->remaining->fetch_sub(1) == 1) {
        for (int i = 0; i < ctx->fanout; i++) {
            queue_push(ctx->queue_out, NULL);
        }
    }

    return NULL;
}

static void* stage_save(void* arg) {
    stage_ctx_t* ctx = static_cast<stage_ctx_t*>(arg);

    while (1) {
        image_t* image = static_cast<image_t*>(queue_pop(ctx->queue_in));
        if (image == NULL) {
            break;
        }

        image_dir_save(ctx->image_dir, image);
        printf(".");
        fflush(stdout);
        image_destroy(image);
    }

    return NULL;
}

/* Nombre de threads dans le bassin d'une étage, proportionnel au nombre de coeurs logiques de
 * la machine plutôt qu'une constante fixe. */
static int stage_thread_count(int total) {
    int threads = (int)(total * STAGE_THREAD_SHARE);
    if (threads < 1) {
        threads = 1;
    }
    if (threads > MAX_STAGE_THREADS) {
        threads = MAX_STAGE_THREADS;
    }
    return threads;
}

int pipeline_pthread(image_dir_t* image_dir) {
    queue_t* q1 = queue_create(QUEUE_SIZE);
    queue_t* q2 = queue_create(QUEUE_SIZE);
    queue_t* q3 = queue_create(QUEUE_SIZE);
    queue_t* q4 = queue_create(QUEUE_SIZE);
    if (q1 == NULL || q2 == NULL || q3 == NULL || q4 == NULL) {
        LOG_ERROR("queue_create");
        if (q1 != NULL) {
            queue_destroy(q1);
        }
        if (q2 != NULL) {
            queue_destroy(q2);
        }
        if (q3 != NULL) {
            queue_destroy(q3);
        }
        if (q4 != NULL) {
            queue_destroy(q4);
        }
        return -1;
    }

    /* q1..q4 are non-NULL and owned by this function from here on: every remaining exit path
     * (below) destroys all four exactly once, so no VLA declaration ever needs a `goto` to reach
     * cleanup (which C forbids jumping over anyway). */
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    int total  = (cores > 0 ? (int)cores : 1) * 2;

    int n_a    = stage_thread_count(total);
    int n_b    = stage_thread_count(total);
    int n_c    = stage_thread_count(total);
    int n_save = stage_thread_count(total);

    std::atomic_int remaining_a = n_a;
    std::atomic_int remaining_b = n_b;
    std::atomic_int remaining_c = n_c;

    stage_ctx_t ctx_load = {
        .image_dir = image_dir, .queue_in = NULL, .queue_out = q1, .remaining = NULL, .fanout = n_a};
    stage_ctx_t ctx_a = {.image_dir = NULL, .queue_in = q1, .queue_out = q2, .remaining = &remaining_a, .fanout = n_b};
    stage_ctx_t ctx_b = {.image_dir = NULL, .queue_in = q2, .queue_out = q3, .remaining = &remaining_b, .fanout = n_c};
    stage_ctx_t ctx_c = {
        .image_dir = NULL, .queue_in = q3, .queue_out = q4, .remaining = &remaining_c, .fanout = n_save};
    stage_ctx_t ctx_save = {.image_dir = image_dir, .queue_in = q4, .queue_out = NULL, .remaining = NULL, .fanout = 0};

    pthread_t thread_load;
    std::vector<pthread_t> threads_a(n_a);
    std::vector<pthread_t> threads_b(n_b);
    std::vector<pthread_t> threads_c(n_c);
    std::vector<pthread_t> threads_save(n_save);

    int started = (pthread_create(&thread_load, NULL, stage_load, &ctx_load) == 0);
    for (int i = 0; started && i < n_a; i++) {
        started = (pthread_create(&threads_a[i], NULL, stage_a, &ctx_a) == 0);
    }
    for (int i = 0; started && i < n_b; i++) {
        started = (pthread_create(&threads_b[i], NULL, stage_b, &ctx_b) == 0);
    }
    for (int i = 0; started && i < n_c; i++) {
        started = (pthread_create(&threads_c[i], NULL, stage_c, &ctx_c) == 0);
    }
    for (int i = 0; started && i < n_save; i++) {
        started = (pthread_create(&threads_save[i], NULL, stage_save, &ctx_save) == 0);
    }
    if (!started) {
        LOG_ERROR_ERRNO("pthread_create");
        queue_destroy(q1);
        queue_destroy(q2);
        queue_destroy(q3);
        queue_destroy(q4);
        return -1;
    }

    pthread_join(thread_load, NULL);
    for (int i = 0; i < n_a; i++) {
        pthread_join(threads_a[i], NULL);
    }
    for (int i = 0; i < n_b; i++) {
        pthread_join(threads_b[i], NULL);
    }
    for (int i = 0; i < n_c; i++) {
        pthread_join(threads_c[i], NULL);
    }
    for (int i = 0; i < n_save; i++) {
        pthread_join(threads_save[i], NULL);
    }

    queue_destroy(q1);
    queue_destroy(q2);
    queue_destroy(q3);
    queue_destroy(q4);

    printf("\n");
    return 0;
}
