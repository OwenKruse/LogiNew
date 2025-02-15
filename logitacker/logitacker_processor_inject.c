#include "logitacker_processor_inject.h"
#include "nrf.h"
#include "logitacker.h"
#include "logitacker_processor.h"
#include "helper.h"
#include "string.h"
#include "logitacker_devices.h"
#include "logitacker_tx_payload_provider.h"
#include "logitacker_unifying.h"
#include "logitacker_tx_pay_provider_string_to_keys.h"
#include "logitacker_tx_payload_provider_press_to_keys.h"
#include "logitacker_tx_payload_provider_mouse.h"
#include "logitacker_flash.h"
#include "logitacker_script_engine.h"
#include "logitacker_options.h"
#include "logitacker_mouse_map.h"

#define NRF_LOG_MODULE_NAME LOGITACKER_PROCESSOR_INJECT

#include "nrf_log.h"
#include "logitacker_tx_payload_provider_string_to_altkeys.h"
#include "logitacker_usb.h"
#include "logitacker_bsp.h"

NRF_LOG_MODULE_REGISTER();

// Note: The receiver accepts ESB keyboard frames from a single device, with a delay shorter than 8ms
// According USB HID input reports seem to be produced, even if the USB host hasn't consumed previous
// input reports. For Windows 7, if tx_delays are smaller than 8ms USB HID input reports get lost (although
// all RF ESB reports are properly acknowledged). For Kali x64 (Debian), even with a tx_delay as short as 1ms,
// all USB HID reports arrive at the target. THIS ONLY HOLDS TRUE FOR UNIFYING RECEIVERS, for G-Series receivers
// or Unifying receivers with a G-Series firmware, even with 1ms tx_delay, no USB HID input reports are lost.

#define INJECT_TX_DELAY_MS_UNIFYING_FAST 1
#define INJECT_TX_DELAY_MS_UNIFYING 8
#define INJECT_TX_DELAY_MS_LIGHTSPEED 1
#define INJECT_RETRANSMIT_BEFORE_FAIL 10

// ToDo: change to initialized -> idle -> working -> idle -> not_initialized (SUCCESS/FAIL states aren't needed, proper events could be fired while processing)
typedef enum {
    INJECT_STATE_IDLE,
    INJECT_STATE_WORKING,
    INJECT_STATE_TASK_SUCCEEDED,
    INJECT_STATE_SCRIPT_SUCCEEDED,
    INJECT_STATE_FAILED,
    INJECT_STATE_NOT_INITIALIZED,
} inject_state_t;

typedef struct {
    uint8_t current_rf_address[5];

    uint8_t base_addr[4];
    uint8_t prefix;

    uint8_t tx_delay_ms;

    app_timer_id_t timer_next_action;
    inject_state_t state;
    nrf_esb_payload_t tmp_tx_payload;

    int retransmit_counter;

    bool execute; //indicates if new tasks are executed immediately (true) or enqueued (false)
    inject_task_t current_task; //current task from queue (header data)
    uint8_t current_task_data[LOGITACKER_SCRIPT_ENGINE_MAX_TASK_DATA_MAX_SIZE]; //current task from queue (content)

    logitacker_devices_unifying_device_t *p_device;
    logitacker_tx_payload_provider_t *p_payload_provider; // depends on current task, provides TX payloads, till task has finished

    bool usb_inject;
} logitacker_processor_inject_ctx_t;

static void processor_inject_hid_keyboard_event_handler(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

static void processor_inject_hid_mouse_event_handler_(logitacker_processor_inject_ctx_t *self, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

static void processor_inject_hid_keyboard_event_handler_(logitacker_processor_inject_ctx_t *self, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);

static void processor_inject_hid_mouse_event_handler(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);


void processor_inject_init_func(logitacker_processor_t *p_processor);

void processor_inject_init_func_(logitacker_processor_inject_ctx_t *self);

void processor_inject_deinit_func(logitacker_processor_t *p_processor);

void processor_inject_deinit_func_(logitacker_processor_inject_ctx_t *self);

void processor_inject_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt);

void processor_inject_esb_handler_func_(logitacker_processor_inject_ctx_t *self, nrf_esb_evt_t *p_esb_event);

void processor_inject_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx);

void processor_inject_timer_handler_func_(logitacker_processor_inject_ctx_t *self, void *p_timer_ctx);

void processor_inject_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event);

void processor_inject_bsp_handler_func_(logitacker_processor_inject_ctx_t *self, bsp_event_t event);

void transfer_state(logitacker_processor_inject_ctx_t *self, inject_state_t new_state);

void logitacker_processor_inject_run_next_task(logitacker_processor_inject_ctx_t *self);

static logitacker_processor_t m_processor = {0};
static logitacker_processor_inject_ctx_t m_static_inject_ctx; //we reuse the same context, alternatively an malloc'ed ctx would allow separate instances

static char addr_str_buff[LOGITACKER_DEVICE_ADDR_STR_LEN] = {0};
static uint8_t tmp_addr[LOGITACKER_DEVICE_ADDR_LEN] = {0};
static logitacker_devices_unifying_device_t tmp_device = {0};
//static bool m_ringbuf_initialized;

logitacker_processor_t *contruct_processor_inject_instance(logitacker_processor_inject_ctx_t *const inject_ctx) {
    m_processor.p_ctx = inject_ctx;
    m_processor.p_init_func = processor_inject_init_func;
    m_processor.p_deinit_func = processor_inject_deinit_func;
    m_processor.p_esb_handler = processor_inject_esb_handler_func;
    m_processor.p_timer_handler = processor_inject_timer_handler_func;
    m_processor.p_bsp_handler = processor_inject_bsp_handler_func;
    m_processor.p_usb_hid_keyboard_event_handler = processor_inject_hid_keyboard_event_handler;
    m_processor.p_usb_hid_mouse_event_handler = processor_inject_hid_mouse_event_handler;

    return &m_processor;
}

static void processor_inject_hid_keyboard_event_handler(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    processor_inject_hid_keyboard_event_handler_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_inst, event);
}



static void processor_inject_hid_keyboard_event_handler_(logitacker_processor_inject_ctx_t *self, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event)
{
    switch (event)
    {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_OUT_REPORT_READY");
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
        {
            NRF_LOG_DEBUG("inject: APP_USBD_HID_USER_EVT_IN_REPORT_DONE");
            self->retransmit_counter = 0;

            if (self->p_payload_provider == NULL) {
                transfer_state(self, INJECT_STATE_IDLE);
                return;
            }

            if (g_logitacker_global_config.bootmode == OPTION_LOGITACKER_BOOTMODE_USB_INJECT &&
                g_logitacker_global_config.usbinject_trigger == OPTION_LOGITACKER_USBINJECT_TRIGGER_ON_LEDUPDATE &&
                !g_logitacker_global_runtime_state.usb_inject_script_triggered) {

                app_timer_start(self->timer_next_action, APP_TIMER_TICKS(LOGITACKER_PROCESSOR_INJECT_USB_LED_TRIGGER_INJECTION_PRE_DELAY_MS), NULL);
                g_logitacker_global_runtime_state.usb_inject_script_triggered = true;
                bsp_board_led_on(LED_G);
            } else {
                // fetch next payload
                if ((*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
                    //next payload retrieved
                    NRF_LOG_DEBUG("New payload retrieved from TX_payload_provider");

                    // schedule payload transmission
                    //app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL); //no delay for USB
                    processor_inject_timer_handler_func_(self, self->timer_next_action);

                } else {
                    // no more payloads, we succeeded
                    transfer_state(self, INJECT_STATE_TASK_SUCCEEDED);
                }
            }


            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}


static void processor_inject_hid_mouse_event_handler(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    processor_inject_hid_mouse_event_handler_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_inst, event);
}

static void processor_inject_hid_mouse_event_handler_(logitacker_processor_inject_ctx_t *self, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event) {
    NRF_LOG_INFO("inject: HID mouse event handler");
    switch (event) {
        case APP_USBD_HID_USER_EVT_OUT_REPORT_READY:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_OUT_REPORT_READY");
            break;
        }
        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
        {
            NRF_LOG_DEBUG("inject: APP_USBD_HID_USER_EVT_IN_REPORT_DONE");
            self->retransmit_counter = 0;

            if (self->p_payload_provider == NULL) {
                transfer_state(self, INJECT_STATE_IDLE);
                return;
            }

            // fetch next payload
            if ((*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
                //next payload retrieved
                NRF_LOG_INFO("New payload retrieved from TX_payload_provider");
                // schedule payload transmission
                //app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL); //no delay for USB
                processor_inject_timer_handler_func_(self, self->timer_next_action);

            } else {
                // no more payloads, we succeeded
                transfer_state(self, INJECT_STATE_TASK_SUCCEEDED);
            }
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_SET_BOOT_PROTO");
            break;
        }
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
        {
            NRF_LOG_INFO("inject: APP_USBD_HID_USER_EVT_SET_REPORT_PROTO");
            break;
        }
        default:
            break;
    }
}

void processor_inject_init_func(logitacker_processor_t *p_processor) {
    processor_inject_init_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx);
}

void processor_inject_deinit_func(logitacker_processor_t *p_processor) {
    processor_inject_deinit_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx);
}

void processor_inject_esb_handler_func(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_evt) {
    processor_inject_esb_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_esb_evt);
}

void processor_inject_timer_handler_func(logitacker_processor_t *p_processor, void *p_timer_ctx) {
    processor_inject_timer_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, p_timer_ctx);
}

void processor_inject_bsp_handler_func(logitacker_processor_t *p_processor, bsp_event_t event) {
    processor_inject_bsp_handler_func_((logitacker_processor_inject_ctx_t *) p_processor->p_ctx, event);
}

void processor_inject_bsp_handler_func_(logitacker_processor_inject_ctx_t *self, bsp_event_t event) {
    // do nothing
}


void processor_inject_init_func_(logitacker_processor_inject_ctx_t *self) {
//    *self->p_logitacker_mainstate = LOGITACKER_MODE_INJECT;

    switch (g_logitacker_global_config.workmode) {
        case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
            self->tx_delay_ms = INJECT_TX_DELAY_MS_LIGHTSPEED;
            break;
        case OPTION_LOGITACKER_WORKMODE_G700:
            self->tx_delay_ms = INJECT_TX_DELAY_MS_LIGHTSPEED;
            break;
        case OPTION_LOGITACKER_WORKMODE_UNIFYING:
            self->tx_delay_ms = INJECT_TX_DELAY_MS_UNIFYING;
            break;
        case OPTION_LOGITACKER_WORKMODE_ALL:
            self->tx_delay_ms = INJECT_TX_DELAY_MS_LIGHTSPEED;
            break;
        case OPTION_LOGITACKER_WORKMODE_G305:
            self->tx_delay_ms = INJECT_TX_DELAY_MS_LIGHTSPEED;
            break;
    }

    helper_addr_to_base_and_prefix(self->base_addr, &self->prefix, self->current_rf_address,
                                   LOGITACKER_DEVICE_ADDR_LEN);

    helper_addr_to_hex_str(addr_str_buff, LOGITACKER_DEVICE_ADDR_LEN, self->current_rf_address);
    NRF_LOG_INFO("Initializing injection mode for %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes
    nrf_esb_set_base_address_0(self->base_addr); // set base addr 0
    nrf_esb_update_prefix(0, self->prefix); // set prefix and enable pipe 0
    nrf_esb_enable_pipes(0x01); //enable pipe 0


    // clear TX/RX payload buffers (just to be sure)
    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload

    // prepare test TX payload (first report will be a pairing request)
    self->tmp_tx_payload.noack = false;
    self->state = INJECT_STATE_IDLE;

    self->retransmit_counter = 0;

    // setup radio as PTX
    nrf_esb_set_mode(NRF_ESB_MODE_PTX);

    switch (g_logitacker_global_config.workmode) {
        case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
            nrf_esb_update_channel_frequency_table_lightspeed();
            break;
        case OPTION_LOGITACKER_WORKMODE_UNIFYING:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
        case OPTION_LOGITACKER_WORKMODE_G700:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
        case OPTION_LOGITACKER_WORKMODE_ALL:
            nrf_esb_update_channel_frequency_table_unifying();
            break;
        case OPTION_LOGITACKER_WORKMODE_G305:
            nrf_esb_update_channel_frequency_table_g305();
            break;
    }

    nrf_esb_enable_all_channel_tx_failover(true); //retransmit payloads on all channels if transmission fails
    nrf_esb_set_all_channel_tx_failover_loop_count(2); //iterate over channels two time before failing
    nrf_esb_set_retransmit_count(1);
    nrf_esb_set_retransmit_delay(250);
    nrf_esb_set_tx_power(NRF_ESB_TX_POWER_8DBM);
}

void processor_inject_deinit_func_(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("Stop injection mode for address %s", addr_str_buff);

    radio_disable_rx_timeout_event(); // disable RX timeouts
    radio_stop_channel_hopping(); // disable channel hopping
    nrf_esb_stop_rx(); //stop rx in case running

    nrf_esb_set_mode(NRF_ESB_MODE_PROMISCOUS); //should disable and end up in idle state

    // set current address for pipe 1
    nrf_esb_enable_pipes(0x00); //disable all pipes

    // reset inner loop count
    self->prefix = 0x00; //unset prefix
    memset(self->base_addr, 0, 4); //unset base address
    memset(self->current_rf_address, 0, 5); //unset RF address

    memset(&self->tmp_tx_payload, 0, sizeof(self->tmp_tx_payload)); //unset TX payload

    self->state = INJECT_STATE_NOT_INITIALIZED;
    self->retransmit_counter = 0;

    //flush_tasks(); //we keep the tasks
    nrf_esb_enable_all_channel_tx_failover(false); // disable all channel failover
}

void processor_inject_timer_handler_func_(logitacker_processor_inject_ctx_t *self, void *p_timer_ctx) {
    if (self->state == INJECT_STATE_WORKING && self->execute) {
        switch (self->current_task.type) {
            case INJECT_TASK_TYPE_DELAY: {
                NRF_LOG_INFO("DELAY end reached");
                //self->current_task.finished = true;
                transfer_state(self, INJECT_STATE_TASK_SUCCEEDED);
                //free_task(self->current_task);
                break;
            }
            case INJECT_TASK_TYPE_PRESS_KEYS:
            case INJECT_TASK_TYPE_TYPE_STRING:
            case INJECT_TASK_TYPE_TYPE_ALTSTRING: {
                // if timer is called, write (and auto transmit) current payload
                // Log the pipe
                NRF_LOG_INFO("TX'ing to pipe %d", self->tmp_tx_payload.pipe);

                if (self->usb_inject) {
                    //write USB HID report
                    if (logitacker_usb_write_keyboard_input_report(self->tmp_tx_payload.data) != NRF_SUCCESS) {
                        NRF_LOG_WARNING("Failed to write report, busy with old report");
                    } else {
                        NRF_LOG_INFO("keyboard report sent to USB");
                    }
                } else {
                    // fix: only append checksum to report if ESB is used
                    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
                    if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
                        NRF_LOG_INFO("Error writing payload");
                    } else {
                        nrf_esb_convert_pipe_to_address(self->tmp_tx_payload.pipe, tmp_addr);
                        helper_addr_to_hex_str(addr_str_buff, 5, tmp_addr);
                        NRF_LOG_DEBUG("TX'ed to %s", nrf_log_push(addr_str_buff));
                    }
                }
                break;
            }
            case INJECT_TASK_TYPE_MOUSE_REPORT: {
                // if timer is called, write (and auto transmit) current payload
                if (self->usb_inject) {
                    //write USB HID report
                    if (logitacker_usb_write_mouse_input_report(self->tmp_tx_payload.data) != NRF_SUCCESS) {
                        NRF_LOG_WARNING("Failed to write mouse report, busy with old report");
                    } else {
                        NRF_LOG_INFO("mouse report sent to USB");
                    }
                } else {
                    // fix: only append checksum to report if ESB is used
                    logitacker_unifying_payload_update_checksum(self->tmp_tx_payload.data, self->tmp_tx_payload.length);
                    if (nrf_esb_write_payload(&self->tmp_tx_payload) != NRF_SUCCESS) {
                        NRF_LOG_INFO("Error writing payload");
                        NRF_LOG_INFO("Payload length: %d", self->tmp_tx_payload.length);
                        NRF_LOG_INFO("Payload pipe: %d", self->tmp_tx_payload.pipe);
                        transfer_state(self, INJECT_STATE_FAILED);

                    } else {
                        NRF_LOG_INFO("Payload length: %d", self->tmp_tx_payload.length);
                        NRF_LOG_INFO("Payload pipe: %d", self->tmp_tx_payload.pipe);

                        nrf_esb_convert_pipe_to_address(self->tmp_tx_payload.pipe, tmp_addr);

                        helper_addr_to_hex_str(addr_str_buff, 5, tmp_addr);
                        NRF_LOG_INFO("TX'ed to %s", nrf_log_push(addr_str_buff));
                    }
                }
                break;

            }
            default:
                NRF_LOG_WARNING("timer event fired for unhandled TASK TYPE: %d", self->current_task.type);
                break;
        }
    }

}

// runs next task if state is transferred back to idle
void transfer_state(logitacker_processor_inject_ctx_t *self, inject_state_t new_state) {
    if (new_state == self->state) return; //no state change
    bool reset_payload_provider = false;
    bool run_next_task = false;

    switch (new_state) {
        case INJECT_STATE_IDLE:
            // stop all actions, send notification to callback
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;
            self->state = new_state;
            reset_payload_provider = true;
            run_next_task = true;
            self->state = new_state; // turn back to idle state
            self->execute = false; //pause execution
            break;
        case INJECT_STATE_TASK_SUCCEEDED:
            // ToDo: callback notify
            NRF_LOG_INFO("inject task succeeded"); // placeholder for callback

            //transfer state to IDLE
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;
            reset_payload_provider = true;
            run_next_task = true;
            //self->state = new_state;
            self->state = INJECT_STATE_IDLE; // turn back to idle state
            break;
        case INJECT_STATE_FAILED:
            // no distinguishing between fail of single task and fail of script, this is the position to abort

            // ToDo: callback notify
            NRF_LOG_INFO("inject task failed"); // placeholder for callback
            app_timer_stop(self->timer_next_action);
            self->retransmit_counter = 0;

            // reset the payload provider
            if (self->p_payload_provider != NULL) (*self->p_payload_provider->p_reset)(self->p_payload_provider);

            //rewind tasks
            logitacker_script_engine_rewind();

            // block further execution
            self->execute = false; //pause execution

            self->state = INJECT_STATE_IDLE; // turn back to idle state

            // carry out fail action
            switch (g_logitacker_global_config.inject_on_fail) {
                case OPTION_AFTER_INJECT_CONTINUE:
                    break;
                case OPTION_AFTER_INJECT_SWITCH_DISCOVERY:
                    logitacker_enter_mode_discovery();
                    break;
                case OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION:
                    logitacker_enter_mode_active_enum(self->current_rf_address);
                case OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION:
                    logitacker_enter_mode_passive_enum(self->current_rf_address);
            }


            break;
        case INJECT_STATE_SCRIPT_SUCCEEDED:
            self->state = INJECT_STATE_IDLE;
            NRF_LOG_INFO("script execution succeeded")
            logitacker_script_engine_flush_tasks();
            switch (g_logitacker_global_config.inject_on_success) {
                case OPTION_AFTER_INJECT_CONTINUE:
                    break;
                case OPTION_AFTER_INJECT_SWITCH_DISCOVERY:
                    logitacker_enter_mode_discovery();
                    break;
                case OPTION_AFTER_INJECT_SWITCH_ACTIVE_ENUMERATION:
                    logitacker_enter_mode_active_enum(self->current_rf_address);
                case OPTION_AFTER_INJECT_SWITCH_PASSIVE_ENUMERATION:
                    logitacker_enter_mode_passive_enum(self->current_rf_address);
            }
            break;
        default:
            self->state = new_state;
            break;
    }

    if (reset_payload_provider) {
        if (self->p_payload_provider != NULL) (*self->p_payload_provider->p_reset)(self->p_payload_provider);
    }


    if (run_next_task) {
        if (self->execute) logitacker_processor_inject_run_next_task(self);
    }
}

void processor_inject_esb_handler_func_(logitacker_processor_inject_ctx_t *self, nrf_esb_evt_t *p_esb_event) {
    if (self->retransmit_counter >= INJECT_RETRANSMIT_BEFORE_FAIL) {
        NRF_LOG_WARNING("Too many retransmissions")
        //self->state = INJECT_STATE_FAILED;
        transfer_state(self, INJECT_STATE_FAILED);
    }

    switch (p_esb_event->evt_id) {
        case NRF_ESB_EVENT_TX_FAILED: {
            NRF_LOG_WARNING("TX FAILED")
            transfer_state(self, INJECT_STATE_TASK_SUCCEEDED);
            break;
        }
        case NRF_ESB_EVENT_TX_SUCCESS_ACK_PAY:
            nrf_esb_flush_rx(); //ignore inbound payloads
            // fall through
        case NRF_ESB_EVENT_TX_SUCCESS: {
            NRF_LOG_DEBUG("TX_SUCCESS");
            self->retransmit_counter = 0;

            if (self->p_payload_provider == NULL) {
                transfer_state(self, INJECT_STATE_IDLE);
                return;
            }

            // fetch next payload
            if ((*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
                //next payload retrieved
                NRF_LOG_DEBUG("New payload retrieved from TX_payload_provider");
                // schedule payload transmission
                app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);

            } else {
                // no more payloads, we succeeded
                transfer_state(self, INJECT_STATE_TASK_SUCCEEDED);
            }

            // schedule next payload

            break;
        }
        case NRF_ESB_EVENT_RX_RECEIVED: {
            NRF_LOG_ERROR("ESB EVENT HANDLER PAIR DEVICE RX_RECEIVED ... !!shouldn't happen!!");
            break;
        }
    }

    return;
}

void logitacker_processor_inject_process_task_string(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process string injection: %s", self->current_task.p_data_c);
    //self->p_payload_provider = new_payload_provider_string(self->p_device, self->current_task.lang, self->current_task.p_data_c);
    self->p_payload_provider = new_payload_provider_string(self->usb_inject,
                                                           self->p_device,
                                                           logitacker_script_engine_get_language_layout(),
                                                           self->current_task.p_data_c);


    //fetch first payload
    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        transfer_state(self, INJECT_STATE_FAILED);
        return;
    }

    transfer_state(self, INJECT_STATE_WORKING);


    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}

void logitacker_processor_inject_process_task_altstring(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process string injection: %s", self->current_task.p_data_c);
    //self->p_payload_provider = new_payload_provider_string(self->p_device, self->current_task.lang, self->current_task.p_data_c);
    self->p_payload_provider = new_payload_provider_altstring(self->usb_inject, self->p_device,
                                                              self->current_task.p_data_c);

    //fetch first payload
    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        transfer_state(self, INJECT_STATE_FAILED);
        return;
    }

    transfer_state(self, INJECT_STATE_WORKING);


    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}

void logitacker_processor_inject_process_task_press(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process key-combo injection: %s", self->current_task.p_data_c);
    self->p_payload_provider = new_payload_provider_press(self->usb_inject,
                                                          self->p_device,
                                                          logitacker_script_engine_get_language_layout(),
                                                          self->current_task.p_data_c);
    //while ((*p_pay_provider->p_get_next)(p_pay_provider, &tmp_pay)) {};

    //fetch first payload
    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        transfer_state(self, INJECT_STATE_FAILED);
        return;
    }

    transfer_state(self, INJECT_STATE_WORKING);


    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}



void logitacker_processor_inject_process_task_mouse(logitacker_processor_inject_ctx_t *self) {
    NRF_LOG_INFO("process mouse injection: %s", self->current_task.p_data_u8[0]);

    logitacker_mouse_map_t *p_mouse_report = NULL;
    bool is_lightspeed = false;

    switch (g_logitacker_global_config.workmode) {
        case OPTION_LOGITACKER_WORKMODE_LIGHTSPEED:
        case OPTION_LOGITACKER_WORKMODE_G700:
        case OPTION_LOGITACKER_WORKMODE_ALL:
        case OPTION_LOGITACKER_WORKMODE_G305:
            is_lightspeed = true;
            break;
        case OPTION_LOGITACKER_WORKMODE_UNIFYING:
            break;
    }
    p_mouse_report = logitacker_mouse_map_get_from_data(self->current_task.p_data_u8);

    if (p_mouse_report == NULL) {
        NRF_LOG_WARNING("failed to map mouse report");
        transfer_state(self, INJECT_STATE_FAILED);
        return;
    }
    self->p_payload_provider = new_payload_provider_mouse(self->usb_inject, self->p_device, p_mouse_report, self->current_task.p_data_u32, is_lightspeed);

    if (!(*self->p_payload_provider->p_get_next)(self->p_payload_provider, &self->tmp_tx_payload)) {
        //failed to fetch first payload
        NRF_LOG_WARNING("failed to fetch initial RF report from payload provider");
        transfer_state(self, INJECT_STATE_FAILED);
        return;
    }


    transfer_state(self, INJECT_STATE_WORKING);

    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(self->tx_delay_ms), NULL);
}



void logitacker_processor_inject_process_task_delay(logitacker_processor_inject_ctx_t *self) {
    uint32_t delay_ms = *self->current_task.p_data_u32;

    NRF_LOG_INFO("process delay injection: %d milliseconds", delay_ms);
    self->p_payload_provider = NULL;
    if (delay_ms == 0) {
        transfer_state(self, INJECT_STATE_TASK_SUCCEEDED); // delay was 0
        return;
    }


    //self->state = INJECT_STATE_WORKING;
    transfer_state(self, INJECT_STATE_WORKING);

    //start injection
    app_timer_start(self->timer_next_action, APP_TIMER_TICKS(delay_ms), NULL);
}


void logitacker_processor_inject_run_next_task(logitacker_processor_inject_ctx_t *self) {
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }

    // pop task
    if (self->state != INJECT_STATE_IDLE) {
        NRF_LOG_ERROR("current task not finished");
        return;
    }
    if (!logitacker_script_engine_read_next_task(&self->current_task, self->current_task_data)) {

        NRF_LOG_INFO("No more tasks scheduled");

        // reset peek pointer to beginning of task buffer
        //ringbuf_peek_rewind(&m_ringbuf);
        logitacker_script_engine_rewind();
        self->execute = false;

        transfer_state(self, INJECT_STATE_SCRIPT_SUCCEEDED);

        //logitacker_enter_mode_discovery();
        return;
    }

    switch (self->current_task.type) {
        case INJECT_TASK_TYPE_PRESS_KEYS:
            logitacker_processor_inject_process_task_press(self);
            break;
        case INJECT_TASK_TYPE_TYPE_STRING:
            logitacker_processor_inject_process_task_string(self);
            break;
        case INJECT_TASK_TYPE_TYPE_ALTSTRING:
            logitacker_processor_inject_process_task_altstring(self);
            break;
        case INJECT_TASK_TYPE_DELAY:
            logitacker_processor_inject_process_task_delay(self);
            break;
        case INJECT_TASK_TYPE_MOUSE_REPORT:
            logitacker_processor_inject_process_task_mouse(self);
            break;
        default:
            NRF_LOG_ERROR("unhandled task type %d, dropped ...", self->current_task.type);
            //self->current_task.finished = true;
            transfer_state(self, INJECT_STATE_FAILED);
            //free_task(self->current_task);
            break;
    }
}

void logitacker_processor_inject_start_execution(logitacker_processor_t *p_processor_inject, bool execute) {
    if (p_processor_inject == NULL) {
        NRF_LOG_ERROR("logitacker processor is NULL");
        return;
    }

    logitacker_processor_inject_ctx_t *self = (logitacker_processor_inject_ctx_t *) p_processor_inject->p_ctx;
    if (self == NULL) {
        NRF_LOG_ERROR("logitacker processor inject context is NULL");
        return;
    }

    self->execute = execute;
    if (self->execute) logitacker_processor_inject_run_next_task(self);
}

logitacker_processor_t *new_processor_inject(uint8_t const *target_rf_address, app_timer_id_t timer_next_action) {
    // initialize context (static in this case, has to use malloc for new instances)
    logitacker_processor_inject_ctx_t *const p_ctx = &m_static_inject_ctx;
    memset(p_ctx, 0, sizeof(*p_ctx)); //replace with malloc for dedicated instance

    //initialize member variables
    memcpy(p_ctx->current_rf_address, target_rf_address, 5);
    p_ctx->timer_next_action = timer_next_action;
    p_ctx->execute = false;

    p_ctx->usb_inject = true;
    for (int i = 0; i < 5; i++) {
        if (target_rf_address[i] != 0x00) {
            p_ctx->usb_inject = false;
            break;
        }
    }

    p_ctx->p_device = NULL;

    if (!p_ctx->usb_inject) {
        logitacker_devices_get_device(&p_ctx->p_device, p_ctx->current_rf_address);
        if (p_ctx->p_device == NULL) {
            NRF_LOG_WARNING("device not found, creating capabilities");
            //tmp_device.is_encrypted = false;
            memcpy(tmp_device.rf_address, p_ctx->current_rf_address, 5);
            p_ctx->p_device = &tmp_device;
        }
    }


    return contruct_processor_inject_instance(&m_static_inject_ctx);
}
