#include <stdint.h>
#include <stdlib.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/core.h"
#include "../coreinfo/coreinfo.h"
#include "../video/hw_render.h"
#include "core_state.h"

static int session_available;

static size_t effective_limit(const size_t caller_limit) {
    size_t limit = coreinfo_state_max_bytes();
    if (caller_limit && caller_limit < limit) limit = caller_limit;
    if (limit > UINT32_MAX) limit = UINT32_MAX;
    return limit;
}

static int valid_size(const size_t size, const size_t limit) {
    return size > 0 && size <= limit;
}

static void quarantine(const char *operation, const char *reason) {
    if (!session_available) return;
    session_available = 0;
    LOG_ERROR(mux_module, "Core-state broker disabled this session after %s: %s", operation, reason);
}

void core_state_session_init(void) {
    session_available = coreinfo_feature_enabled(coreinfo_feature_save_states)
        && current_core.retro_serialize_size && current_core.retro_serialize && current_core.retro_unserialize;
    if (coreinfo_feature_enabled(coreinfo_feature_save_states) && !session_available)
        LOG_WARN(mux_module, "Core-state broker unavailable because the core does not provide the complete state API");
}

int core_state_available(void) {
    return session_available;
}

size_t core_state_reported_size(const size_t caller_limit, const char *operation) {
    if (!session_available) return 0;
    const size_t limit = effective_limit(caller_limit);
    hw_render_bridge_enter_core_call();
    const size_t size = current_core.retro_serialize_size();
    hw_render_bridge_exit_core_call();
    if (!valid_size(size, limit)) {
        LOG_ERROR(mux_module, "Core-state %s reported=%zu limit=%zu result=invalid", operation, size, limit);
        quarantine(operation, size ? "reported state exceeds the safe limit" : "reported an empty state");
        return 0;
    }
    return size;
}

int core_state_capture_prefixed(
    struct core_state_buffer *buffer, const size_t prefix_size, size_t requested_size, const size_t caller_limit,
    const int verify_after, const char *operation
) {
    if (!session_available || !buffer) return -1;

    const size_t limit = effective_limit(caller_limit);
    hw_render_bridge_enter_core_call();
    size_t size = requested_size ? requested_size : current_core.retro_serialize_size();
    if (!valid_size(size, limit)) {
        hw_render_bridge_exit_core_call();
        LOG_ERROR(mux_module, "Core-state %s reported=%zu limit=%zu result=invalid", operation, size, limit);
        quarantine(operation, size ? "requested state exceeds the safe limit" : "reported an empty state");
        return -1;
    }

    if (prefix_size > SIZE_MAX - size) {
        hw_render_bridge_exit_core_call();
        LOG_ERROR(mux_module, "Core-state %s allocation size overflow", operation);
        return -1;
    }
    size_t allocation_size = prefix_size + size;
    if (allocation_size > buffer->capacity) {
        uint8_t *grown = realloc(buffer->data, allocation_size);
        if (!grown) {
            hw_render_bridge_exit_core_call();
            LOG_ERROR(mux_module, "Core-state %s could not allocate %zu bytes", operation, size);
            return -1;
        }
        buffer->data = grown;
        buffer->capacity = allocation_size;
    }

    int okay = current_core.retro_serialize(buffer->data + prefix_size, size);
    if (!okay) {
        const size_t retry_size = current_core.retro_serialize_size();
        if (retry_size != size && valid_size(retry_size, limit)) {
            if (prefix_size > SIZE_MAX - retry_size) {
                hw_render_bridge_exit_core_call();
                LOG_ERROR(mux_module, "Core-state %s retry allocation size overflow", operation);
                return -1;
            }
            allocation_size = prefix_size + retry_size;
            if (allocation_size > buffer->capacity) {
                uint8_t *grown = realloc(buffer->data, allocation_size);
                if (!grown) {
                    hw_render_bridge_exit_core_call();
                    LOG_ERROR(mux_module, "Core-state %s could not resize to %zu bytes", operation, retry_size);
                    return -1;
                }
                buffer->data = grown;
                buffer->capacity = allocation_size;
            }
            LOG_WARN(mux_module, "Core-state %s changed size from %zu to %zu; retrying", operation, size, retry_size);
            size = retry_size;
            okay = current_core.retro_serialize(buffer->data + prefix_size, size);
        }
    }

    size_t settled = size;
    if (okay && verify_after) settled = current_core.retro_serialize_size();
    hw_render_bridge_exit_core_call();

    if (!okay || (verify_after && settled != size)) {
        LOG_ERROR(
            mux_module, "Core-state %s reported=%zu supplied=%zu limit=%zu result=%s", operation, settled, size,
            limit, okay ? "changed" : "rejected"
        );
        quarantine(operation, okay ? "state size changed during capture" : "core rejected serialisation");
        return -1;
    }

    buffer->size = size;
    LOG_DEBUG(
        mux_module, "Core-state %s reported=%zu supplied=%zu limit=%zu result=captured", operation, settled, size,
        limit
    );
    return 0;
}

int core_state_capture(
    struct core_state_buffer *buffer, const size_t requested_size, const size_t caller_limit, const int verify_after,
    const char *operation
) {
    return core_state_capture_prefixed(buffer, 0, requested_size, caller_limit, verify_after, operation);
}

int core_state_restore(const void *data, const size_t size, const size_t caller_limit, const char *operation) {
    if (!session_available || !data) return -1;

    const size_t limit = effective_limit(caller_limit);
    if (!valid_size(size, limit)) {
        LOG_ERROR(mux_module, "Core-state %s supplied=%zu limit=%zu result=invalid", operation, size, limit);
        return -1;
    }

    hw_render_bridge_enter_core_call();
    const size_t reported = current_core.retro_serialize_size();
    if (!valid_size(reported, limit)) {
        hw_render_bridge_exit_core_call();
        LOG_ERROR(mux_module, "Core-state %s reported=%zu supplied=%zu limit=%zu result=invalid", operation, reported, size, limit);
        quarantine(operation, reported ? "reported state exceeds the safe limit" : "reported an empty state");
        return -1;
    }

    if (reported != size && coreinfo_state_load_policy() == coreinfo_state_load_exact) {
        hw_render_bridge_exit_core_call();
        LOG_ERROR(
            mux_module, "Core-state %s reported=%zu supplied=%zu limit=%zu policy=exact result=refused", operation,
            reported, size, limit
        );
        return -1;
    }

    const int okay = current_core.retro_unserialize(data, size);
    hw_render_bridge_exit_core_call();
    LOG_DEBUG(
        mux_module, "Core-state %s reported=%zu supplied=%zu limit=%zu policy=%s result=%s", operation, reported,
        size, limit, coreinfo_state_load_policy() == coreinfo_state_load_exact ? "exact" : "core",
        okay ? "restored" : "rejected"
    );
    return okay ? 0 : -1;
}

void core_state_buffer_release(struct core_state_buffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->capacity = 0;
    buffer->size = 0;
}
