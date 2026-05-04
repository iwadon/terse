#include "terse.h"
#include "terse_handle.h"
#include "terse_test_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef TERSE_ENABLE_TEST_MODE

void record_call(terse_handle_t handle, terse_call_type_t type, const void *data, size_t data_size)
{
	if (!handle || !handle->test_state || !handle->test_state->recording) {
		return;
	}
	terse_test_state_t *ts = handle->test_state;
	if (ts->call_count >= ts->call_capacity) {
		int new_capacity = (ts->call_capacity == 0) ? 16 : (ts->call_capacity * 2);
		terse_call_record_t *new_calls = realloc(ts->calls, sizeof(terse_call_record_t) * (size_t)new_capacity);
		if (!new_calls) {
			return;
		}
		ts->calls = new_calls;
		ts->call_capacity = new_capacity;
	}
	terse_call_record_t *rec = &ts->calls[ts->call_count];
	memset(rec, 0, sizeof(*rec));
	rec->type = type;
	if (data && data_size > 0) {
		memcpy(&rec->data, data, data_size < sizeof(rec->data) ? data_size : sizeof(rec->data));
	}
	ts->call_count++;
}

void terse_test_state_init(terse_handle_t handle)
{
	if (!handle) {
		return;
	}
	handle->test_state = malloc(sizeof(terse_test_state_t));
	if (handle->test_state) {
		memset(handle->test_state, 0, sizeof(terse_test_state_t));
		handle->test_state->recording = 0;
		handle->test_state->calls = NULL;
		handle->test_state->call_count = 0;
		handle->test_state->call_capacity = 0;
		handle->test_state->mock_caps_enabled = 0;
		handle->test_state->mock_size_enabled = 0;
		handle->test_state->mock_events = NULL;
		handle->test_state->mock_event_count = 0;
		handle->test_state->mock_event_read_index = 0;
	}
}

void terse_test_state_destroy(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		return;
	}
	free(handle->test_state->calls);
	free(handle->test_state->mock_events);
	free(handle->test_state);
	handle->test_state = NULL;
}

int terse_test_start_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 1;
	return 0;
}

int terse_test_stop_recording(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->recording = 0;
	return 0;
}

const terse_call_record_t *terse_test_get_calls(terse_handle_t handle, int *out_count)
{
	if (!handle || !handle->test_state || !out_count) {
		if (out_count) {
			*out_count = 0;
		}
		return NULL;
	}
	*out_count = handle->test_state->call_count;
	return handle->test_state->calls;
}

int terse_test_clear_calls(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->call_count = 0;
	return 0;
}

int terse_test_mock_capabilities(terse_handle_t handle, const terse_capabilities_t *caps)
{
	if (!handle || !handle->test_state || !caps) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps = *caps;
	handle->test_state->mock_caps_enabled = 1;
	return 0;
}

int terse_test_mock_size(terse_handle_t handle, int rows, int cols)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_rows = rows;
	handle->test_state->mock_cols = cols;
	handle->test_state->mock_size_enabled = 1;
	return 0;
}

int terse_test_mock_events(terse_handle_t handle, const terse_event_t *events, int count)
{
	if (!handle || !handle->test_state || !events || count < 0) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;

	if (count > 0) {
		handle->test_state->mock_events = malloc(sizeof(terse_event_t) * (size_t)count);
		if (!handle->test_state->mock_events) {
			errno = ENOMEM;
			return TERSE_ERR_OUT_OF_MEMORY;
		}
		memcpy(handle->test_state->mock_events, events, sizeof(terse_event_t) * (size_t)count);
		handle->test_state->mock_event_count = count;
	}
	return 0;
}

int terse_test_reset_mocks(terse_handle_t handle)
{
	if (!handle || !handle->test_state) {
		errno = EINVAL;
		return TERSE_ERR_INVALID_ARGUMENT;
	}
	handle->test_state->mock_caps_enabled = 0;
	handle->test_state->mock_size_enabled = 0;
	free(handle->test_state->mock_events);
	handle->test_state->mock_events = NULL;
	handle->test_state->mock_event_count = 0;
	handle->test_state->mock_event_read_index = 0;
	return 0;
}

#endif // TERSE_ENABLE_TEST_MODE
