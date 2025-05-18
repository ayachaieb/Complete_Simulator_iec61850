#include "app/state_machine.h"
#include "app/state_common.h"
#include "app/state_idle.h"
#include "app/state_init.h"
#include "app/state_running.h"
#include "app/state_stop.h"
#include "app/timer.h"
#include "common/trace.h"
#include "common/defines.h"
#include "common/assert_handler.h"
#include "common/enum_to_string.h"
#include "common/ring_buffer.h"
#include "common/sleep.h"

struct state_transition
{
    state_e from;
    state_event_e event;
    state_e to;
};

static const struct state_transition state_transitions[] = {
    { STATE_IDLE, STATE_EVENT_NONE, STATE_IDLE }, 
    { STATE_IDLE, STATE_EVENT_start_config, STATE_INIT },
    { STATE_IDLE, STATE_EVENT_shutdown, STATE_STOP },
    { STATE_INIT, STATE_EVENT_start_simulation, STATE_RUNNING },
    { STATE_INIT, STATE_EVENT_shutdown, STATE_STOP },
    { STATE_INIT, STATE_EVENT_config_failed, STATE_IDLE },
    { STATE_RUNNING, STATE_EVENT_stop_simulation, STATE_IDLE },
    { STATE_RUNNING, STATE_EVENT_pause_simulation, STATE_INIT },
    { STATE_RUNNING, STATE_EVENT_shutdown, STATE_STOP },
    { STATE_RUNNING, STATE_EVENT_NONE, STATE_RUNNING }
};

struct state_machine_data
{
    state_e state;
    struct state_common_data common;
    struct state_idle_data idle;
    struct state_init_data init;
    struct state_running_data running;
    struct state_stop_data stop;
    state_event_e internal_event;
    timer_t timer;
    struct ring_buffer input_history;
};

static inline bool has_internal_event(const struct state_machine_data *data)
{
    return data->internal_event != STATE_EVENT_NONE;
}

static inline state_event_e take_internal_event(struct state_machine_data *data)
{
    ASSERT(has_internal_event(data));
    const state_event_e event = data->internal_event;
    data->internal_event = STATE_EVENT_NONE;
    return event;
}

void state_machine_post_internal_event(struct state_machine_data *data, state_event_e event)
{
    ASSERT(!has_internal_event(data));
    data->internal_event = event;
}

static void state_enter(struct state_machine_data *data, state_e from, state_event_e event,
                        state_e to)
{
    if (from != to) {
        timer_clear(&data->timer);
        data->state = to;
        TRACE("%s to %s (%s)", state_to_string(from), state_to_string(event),
              state_event_to_string(to));
    }
    switch (to) {
    case STATE_IDLE:
        state_idle_enter(&(data->idle), from, event);
        break;
    case STATE_INIT:
        state_init_enter(&(data->init), from, event);
        break;
    case STATE_RUNNING:
        state_running_enter(&(data->running), from, event);
        break;
    case STATE_STOP:
        state_stop_enter(&(data->stop), from, event);
        break;
    default:
        ASSERT(0); // Unknown state
    }
}

static inline void process_event(struct state_machine_data *data, state_event_e next_event)
{
    for (uint16_t i = 0; i < ARRAY_SIZE(state_transitions); i++) {
        if (data->state == state_transitions[i].from && next_event == state_transitions[i].event) {
            state_enter(data, state_transitions[i].from, next_event, state_transitions[i].to);
            return;
        }
    }
    ASSERT(0); // No valid transition found
}

static inline state_event_e process_input(struct state_machine_data *data)
{
    // Placeholder for your input processing from simulator backend
    // For now, just return internal event or NONE
    if (has_internal_event(data)) {
        return take_internal_event(data);
    }
    return STATE_EVENT_NONE;
}

static inline void state_machine_init(struct state_machine_data *data)
{
    data->state = STATE_IDLE;
    data->common.state_machine_data = data;
    data->common.timer = &data->timer;
    timer_clear(&data->timer);
    data->internal_event = STATE_EVENT_NONE;
    data->idle.common = &data->common;
    data->init.common = &data->common;
    data->running.common = &data->common;
    data->stop.common = &data->common;

    state_idle_init(&data->idle);
    state_init_init(&data->init);
    state_running_init(&data->running);
    state_stop_init(&data->stop);
}

#define INPUT_HISTORY_BUFFER_SIZE (6u)
void state_machine_run(void)
{
    struct state_machine_data data;

    LOCAL_RING_BUFFER(input_history, INPUT_HISTORY_BUFFER_SIZE, struct input);
    data.input_history = input_history;
    data.common.input_history = &data.input_history;

    state_machine_init(&data);

    while (1) {
        const state_event_e next_event = process_input(&data);
        process_event(&data, next_event);
        sleep_ms(1);
    }
}