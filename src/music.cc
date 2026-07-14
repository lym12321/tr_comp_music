#include "music/music.h"

#include "bsp/buzzer.h"
#include "bsp/sys.h"
#include "FreeRTOS.h"
#include "task.h"

namespace music {
    namespace {
        constexpr uint32_t update_signal = 1;
        constexpr uint32_t player_stack_depth = 256;

        struct request_t {
            const tone_t *tones = nullptr;
            std::size_t size = 0;
            float duty_cycle = 0.0f;
        };

        StaticTask_t player_task_buffer;
        StackType_t player_stack[player_stack_depth];
        TaskHandle_t player_task = nullptr;
        request_t request;
        uint32_t revision = 0;
        bool active = false;

        bool notify_player() {
            if (bsp_sys_in_isr()) {
                BaseType_t task_woken = pdFALSE;
                const BaseType_t result = xTaskNotifyFromISR(
                    player_task, update_signal, eSetBits, &task_woken
                );
                portYIELD_FROM_ISR(task_woken);
                return result == pdPASS;
            }
            return xTaskNotify(player_task, update_signal, eSetBits) == pdPASS;
        }

        bool wait_for_update(TickType_t timeout = portMAX_DELAY) {
            uint32_t value = 0;
            return xTaskNotifyWait(0, update_signal, &value, timeout) == pdTRUE
                && (value & update_signal) != 0;
        }

        request_t latest_request(uint32_t &current_revision) {
            request_t current;

            for (;;) {
                const unsigned long state = bsp_sys_enter_critical();
                current = request;
                current_revision = revision;
                bsp_sys_exit_critical(state);

                wait_for_update(0);

                const unsigned long check_state = bsp_sys_enter_critical();
                const bool unchanged = revision == current_revision;
                bsp_sys_exit_critical(check_state);
                if (unchanged) return current;
            }
        }

        void player(void *) {
            for (;;) {
                wait_for_update();

                for (;;) {
                    uint32_t current_revision;
                    const request_t current = latest_request(current_revision);

                    bsp_buzzer_quiet();
                    if (current.tones == nullptr) break;

                    bool interrupted = false;
                    for (std::size_t i = 0; i < current.size; ++i) {
                        if (wait_for_update(0)) {
                            interrupted = true;
                            break;
                        }

                        const tone_t &tone = current.tones[i];
                        const bsp_status_t status = tone.frequency_hz == 0
                            ? bsp_buzzer_quiet()
                            : bsp_buzzer_alarm(tone.frequency_hz, current.duty_cycle);

                        if (status != BSP_STATUS_OK) break;
                        if (wait_for_update(pdMS_TO_TICKS(tone.duration_ms))) {
                            interrupted = true;
                            break;
                        }
                    }

                    bsp_buzzer_quiet();
                    if (interrupted) continue;

                    const unsigned long finish_state = bsp_sys_enter_critical();
                    const bool completed = revision == current_revision;
                    if (completed) {
                        request = {};
                        active = false;
                    }
                    bsp_sys_exit_critical(finish_state);
                    if (completed) break;
                }
            }
        }
    }

    bool init() {
        const unsigned long state = bsp_sys_enter_critical();
        if (player_task == nullptr) {
            if (bsp_buzzer_quiet() == BSP_STATUS_OK) {
                player_task = xTaskCreateStatic(
                    player, "music", player_stack_depth, nullptr, tskIDLE_PRIORITY + 1,
                    player_stack, &player_task_buffer
                );
            }
        }
        const bool result = player_task != nullptr;
        bsp_sys_exit_critical(state);
        return result;
    }

    bool play(std::span<const tone_t> tones, float duty_cycle) {
        if (tones.empty() || !(duty_cycle > 0.0f && duty_cycle <= 1.0f)) return false;

        const unsigned long state = bsp_sys_enter_critical();
        if (player_task == nullptr) {
            bsp_sys_exit_critical(state);
            return false;
        }
        request = { tones.data(), tones.size(), duty_cycle };
        ++revision;
        active = true;
        const bool notified = notify_player();
        bsp_sys_exit_critical(state);

        return notified;
    }

    void stop() {
        const unsigned long state = bsp_sys_enter_critical();
        if (player_task == nullptr) {
            bsp_sys_exit_critical(state);
            return;
        }
        request = {};
        ++revision;
        active = false;
        notify_player();
        bsp_sys_exit_critical(state);
    }

    bool busy() {
        const unsigned long state = bsp_sys_enter_critical();
        const bool result = active;
        bsp_sys_exit_critical(state);
        return result;
    }
}
