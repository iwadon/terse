#ifndef TERSE_TEST_INTERNAL_H_INCLUDED
#define TERSE_TEST_INTERNAL_H_INCLUDED

#include "terse_test.h"

#ifdef TERSE_ENABLE_TEST_MODE

/* Internal test state structure.
 * This is only used internally and not exposed to public API.
 */
typedef struct terse_test_state {
	int recording;
	terse_call_record_t *calls;
	int call_count;
	int call_capacity;
	terse_capabilities_t mock_caps;
	int mock_caps_enabled;
	int mock_rows;
	int mock_cols;
	int mock_size_enabled;
	terse_event_t *mock_events;
	int mock_event_count;
	int mock_event_read_index;
} terse_test_state_t;

/* Internal function for recording API calls (defined in terse.c).
 * This is shared across implementation modules that need to record calls.
 */
void record_call(terse_handle_t handle, terse_call_type_t type, const void *data, size_t data_size);

#endif // TERSE_ENABLE_TEST_MODE

#endif // TERSE_TEST_INTERNAL_H_INCLUDED
