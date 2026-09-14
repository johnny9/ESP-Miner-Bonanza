#include "asic_job_store.h"

#include "esp_heap_caps.h"

#include <string.h>

static void clear_entry(asic_job_store_entry_t *entry)
{
    if (entry == NULL) return;
    memset(entry, 0, sizeof(*entry));
}

bool asic_job_store_init(asic_job_store_t *store)
{
    return asic_job_store_init_with_caps(store, MALLOC_CAP_DEFAULT);
}

bool asic_job_store_init_with_caps(asic_job_store_t *store,
                                   uint32_t memory_caps)
{
    if (store == NULL) return false;
    memset(store, 0, sizeof(*store));
    store->entries = heap_caps_calloc(
        ASIC_JOB_STORE_CAPACITY, sizeof(*store->entries), memory_caps);
    if (store->entries == NULL) return false;
    store->capacity = ASIC_JOB_STORE_CAPACITY;
    store->next_generation = 1;
    if (pthread_mutex_init(&store->lock, NULL) != 0) {
        heap_caps_free(store->entries);
        memset(store, 0, sizeof(*store));
        return false;
    }
    if (pthread_mutex_init(&store->submission_lock, NULL) != 0) {
        pthread_mutex_destroy(&store->lock);
        heap_caps_free(store->entries);
        memset(store, 0, sizeof(*store));
        return false;
    }
    return true;
}

void asic_job_store_destroy(asic_job_store_t *store)
{
    if (store == NULL) return;
    if (store->entries == NULL) {
        memset(store, 0, sizeof(*store));
        return;
    }
    asic_job_store_invalidate_all(store);
    pthread_mutex_destroy(&store->submission_lock);
    pthread_mutex_destroy(&store->lock);
    heap_caps_free(store->entries);
    memset(store, 0, sizeof(*store));
}

uint64_t asic_job_store_activate_local(asic_job_store_t *store)
{
    pthread_mutex_lock(&store->lock);
    /* Never recycle a local identity, even in the theoretical wrap case. */
    store->local_active = store->local_generation != UINT64_MAX;
    uint64_t generation = store->local_active ? ++store->local_generation : 0;
    pthread_mutex_unlock(&store->lock);
    return generation;
}

void asic_job_store_cancel_local(asic_job_store_t *store)
{
    pthread_mutex_lock(&store->lock);
    store->local_active = false;
    pthread_mutex_unlock(&store->lock);
}

bool asic_job_store_local_is_current(asic_job_store_t *store, uint64_t generation)
{
    pthread_mutex_lock(&store->lock);
    bool current = store->local_active && generation != 0 &&
                   generation == store->local_generation;
    pthread_mutex_unlock(&store->lock);
    return current;
}

void asic_job_store_begin_submission(asic_job_store_t *store,
                                     const asic_job_t *job,
                                     const asic_job_context_t *context)
{
    pthread_mutex_lock(&store->submission_lock);
    pthread_mutex_lock(&store->lock);
    store->submission_job = job;
    store->submission_context = *context;
    pthread_mutex_unlock(&store->lock);
}

void asic_job_store_end_submission(asic_job_store_t *store)
{
    pthread_mutex_lock(&store->lock);
    store->submission_job = NULL;
    memset(&store->submission_context, 0, sizeof(store->submission_context));
    pthread_mutex_unlock(&store->lock);
    pthread_mutex_unlock(&store->submission_lock);
}

bool asic_job_store_submission_context(asic_job_store_t *store,
                                       const asic_job_t *job,
                                       asic_job_context_t *context)
{
    if (store == NULL || store->entries == NULL || job == NULL || context == NULL)
        return false;
    pthread_mutex_lock(&store->lock);
    bool bound = store->submission_job == job;
    *context = bound ? store->submission_context :
        (asic_job_context_t){.job_version = job->version};
    pthread_mutex_unlock(&store->lock);
    return bound;
}

static bool store_entry(asic_job_store_t *store, uint8_t slot,
                        asic_work_handle_t handle,
                        const asic_job_t *template)
{
    asic_job_store_entry_t replacement = {
        .valid = true,
        .handle = handle,
    };
    replacement.template = *template;
    replacement.context = store->submission_job == template ?
        store->submission_context :
        (asic_job_context_t){.job_version = template->version};

    clear_entry(&store->entries[slot]);
    store->entries[slot] = replacement;
    return true;
}

bool asic_job_store_store_slot(asic_job_store_t *store, uint8_t slot,
                               const asic_job_t *template,
                               asic_work_handle_t *handle)
{
    if (store == NULL || store->entries == NULL ||
        slot >= store->capacity || template == NULL) {
        return false;
    }

    pthread_mutex_lock(&store->lock);
    bool stored = store_entry(store, slot, slot, template);
    pthread_mutex_unlock(&store->lock);
    if (stored && handle != NULL) *handle = slot;
    return stored;
}

bool asic_job_store_store_generated(asic_job_store_t *store,
                                    const asic_job_t *template,
                                    asic_work_handle_t *handle)
{
    if (store == NULL || store->entries == NULL || store->capacity == 0 ||
        template == NULL) return false;

    pthread_mutex_lock(&store->lock);
    uint8_t slot = (uint8_t)(store->next_slot++ % store->capacity);
    uint64_t generation = store->next_generation++;
    if (store->next_generation == 0) store->next_generation = 1;
    asic_work_handle_t generated = (generation << 8) | slot;
    bool stored = store_entry(store, slot, generated, template);
    pthread_mutex_unlock(&store->lock);
    if (stored && handle != NULL) *handle = generated;
    return stored;
}

bool asic_job_store_snapshot(asic_job_store_t *store,
                             asic_work_handle_t handle,
                             asic_job_t *snapshot)
{
    return asic_job_store_snapshot_with_context(store, handle, snapshot, NULL);
}

bool asic_job_store_snapshot_with_context(asic_job_store_t *store,
                                          asic_work_handle_t handle,
                                          asic_job_t *snapshot,
                                          asic_job_context_t *context)
{
    if (store == NULL || store->entries == NULL || snapshot == NULL ||
        handle == ASIC_WORK_HANDLE_INVALID) {
        return false;
    }

    uint8_t slot = (uint8_t)(handle & 0xff);
    if (slot >= store->capacity) return false;
    memset(snapshot, 0, sizeof(*snapshot));
    pthread_mutex_lock(&store->lock);
    asic_job_store_entry_t *entry = &store->entries[slot];
    bool found = entry->valid && entry->handle == handle;
    if (found) {
        *snapshot = entry->template;
        if (context != NULL) *context = entry->context;
    }
    pthread_mutex_unlock(&store->lock);
    return found;
}

bool asic_job_store_contains(asic_job_store_t *store,
                             asic_work_handle_t handle)
{
    if (store == NULL || store->entries == NULL ||
        handle == ASIC_WORK_HANDLE_INVALID) return false;
    uint8_t slot = (uint8_t)(handle & 0xff);
    if (slot >= store->capacity) return false;
    pthread_mutex_lock(&store->lock);
    const asic_job_store_entry_t *entry = &store->entries[slot];
    bool found = entry->valid && entry->handle == handle;
    pthread_mutex_unlock(&store->lock);
    return found;
}

bool asic_job_store_release(asic_job_store_t *store,
                            asic_work_handle_t handle)
{
    if (store == NULL || store->entries == NULL ||
        handle == ASIC_WORK_HANDLE_INVALID) return false;
    uint8_t slot = (uint8_t)(handle & 0xff);
    if (slot >= store->capacity) return false;
    pthread_mutex_lock(&store->lock);
    asic_job_store_entry_t *entry = &store->entries[slot];
    bool released = entry->valid && entry->handle == handle;
    if (released) clear_entry(entry);
    pthread_mutex_unlock(&store->lock);
    return released;
}

void asic_job_store_invalidate_all(asic_job_store_t *store)
{
    if (store == NULL || store->entries == NULL) return;
    pthread_mutex_lock(&store->lock);
    for (size_t i = 0; i < store->capacity; ++i) {
        clear_entry(&store->entries[i]);
    }
    store->next_slot = 0;
    if (++store->next_generation == 0) store->next_generation = 1;
    pthread_mutex_unlock(&store->lock);
}
