#include "music/music.h"

#include "bsp/buzzer.h"
#include "bsp/sys.h"
#include "FreeRTOS.h"
#include "music_terminal.h"
#include "semphr.h"
#include "task.h"

namespace music {
    namespace {
        constexpr uint32_t update_signal = 1;
        constexpr uint32_t player_stack_depth = 256;

        struct blocking_waiter_t {
            StaticSemaphore_t storage;
            SemaphoreHandle_t semaphore;
            bool completed = false;
        };

        struct request_t {
            const tone_t *tones = nullptr;
            std::size_t size = 0;
            float duty_cycle = 0.0f;
            blocking_waiter_t *waiter = nullptr;
        };

        StaticTask_t player_task_buffer;
        StackType_t player_stack[player_stack_depth];
        TaskHandle_t player_task = nullptr;
        request_t request;
        uint32_t revision = 0;
        bool active = false;
        bool paused = false;

        struct playback_state_t {
            request_t request;
            uint32_t revision;
            bool paused;
        };

        void notify_player() {
            if (bsp_sys_in_isr()) {
                BaseType_t task_woken = pdFALSE;
                (void) xTaskNotifyFromISR(
                    player_task, update_signal, eSetBits, &task_woken
                );
                portYIELD_FROM_ISR(task_woken);
            } else {
                (void) xTaskNotify(player_task, update_signal, eSetBits);
            }
        }

        bool wait_for_update(TickType_t timeout = portMAX_DELAY) {
            uint32_t value = 0;
            return xTaskNotifyWait(0, update_signal, &value, timeout) == pdTRUE
                && (value & update_signal) != 0;
        }

        playback_state_t latest_state() {
            const unsigned long state = bsp_sys_enter_critical();
            (void) wait_for_update(0);
            const playback_state_t current { request, revision, paused };
            bsp_sys_exit_critical(state);
            return current;
        }

        bool set_paused(bool value) {
            const unsigned long state = bsp_sys_enter_critical();
            if (player_task == nullptr || !active || paused == value) {
                bsp_sys_exit_critical(state);
                return false;
            }
            paused = value;
            notify_player();
            bsp_sys_exit_critical(state);
            return true;
        }

        void finish_waiter(blocking_waiter_t *waiter, bool completed) {
            if (waiter == nullptr) return;
            waiter->completed = completed;

            if (bsp_sys_in_isr()) {
                BaseType_t task_woken = pdFALSE;
                xSemaphoreGiveFromISR(waiter->semaphore, &task_woken);
                portYIELD_FROM_ISR(task_woken);
            } else {
                xSemaphoreGive(waiter->semaphore);
            }
        }

        bool start_playback(
            std::span<const tone_t> tones, float duty_cycle, blocking_waiter_t *waiter
        ) {
            if (tones.empty() || !(duty_cycle > 0.0f && duty_cycle <= 1.0f)) return false;

            const unsigned long state = bsp_sys_enter_critical();
            if (player_task == nullptr) {
                bsp_sys_exit_critical(state);
                return false;
            }
            finish_waiter(request.waiter, false);
            request = { tones.data(), tones.size(), duty_cycle, waiter };
            ++revision;
            active = true;
            paused = false;
            notify_player();
            bsp_sys_exit_critical(state);
            return true;
        }

        void player(void *) {
            request_t current;
            uint32_t current_revision = 0;
            std::size_t tone_index = 0;
            bool failed = false;

            for (;;) {
                wait_for_update();

                for (;;) {
                    const playback_state_t state = latest_state();
                    if (state.revision != current_revision) {
                        current = state.request;
                        current_revision = state.revision;
                        tone_index = 0;
                        failed = false;
                    }

                    if (bsp_buzzer_quiet() != BSP_STATUS_OK) failed = true;
                    if (current.tones == nullptr || state.paused) break;

                    bool interrupted = false;
                    while (tone_index < current.size) {
                        if (wait_for_update(0)) {
                            interrupted = true;
                            break;
                        }

                        const tone_t &tone = current.tones[tone_index];
                        const bsp_status_t status = tone.frequency_hz == 0
                            ? bsp_buzzer_quiet()
                            : bsp_buzzer_alarm(tone.frequency_hz, current.duty_cycle);

                        if (status != BSP_STATUS_OK) {
                            failed = true;
                            tone_index = current.size;
                            break;
                        }
                        if (wait_for_update(pdMS_TO_TICKS(tone.duration_ms))) {
                            interrupted = true;
                            break;
                        }
                        ++tone_index;
                    }

                    if (bsp_buzzer_quiet() != BSP_STATUS_OK) failed = true;
                    if (interrupted) continue;

                    const unsigned long finish_state = bsp_sys_enter_critical();
                    const bool completed = revision == current_revision;
                    if (completed) {
                        finish_waiter(current.waiter, !failed);
                        request = {};
                        active = false;
                        paused = false;
                    }
                    bsp_sys_exit_critical(finish_state);
                    if (completed) break;
                }
            }
        }
    }

    bool init() {
        bool created = false;
        const unsigned long state = bsp_sys_enter_critical();
        if (player_task == nullptr) {
            if (bsp_buzzer_quiet() == BSP_STATUS_OK) {
                player_task = xTaskCreateStatic(
                    player, "music", player_stack_depth, nullptr, tskIDLE_PRIORITY + 1,
                    player_stack, &player_task_buffer
                );
                created = player_task != nullptr;
            }
        }
        const bool result = player_task != nullptr;
        bsp_sys_exit_critical(state);
        if (created) internal::register_terminal_command();
        return result;
    }

    bool play(std::span<const tone_t> tones, float duty_cycle) {
        return start_playback(tones, duty_cycle, nullptr);
    }

    bool play_blocking(std::span<const tone_t> tones, float duty_cycle) {
        if (bsp_sys_in_isr()) return false;

        blocking_waiter_t waiter;
        waiter.semaphore = xSemaphoreCreateBinaryStatic(&waiter.storage);
        if (waiter.semaphore == nullptr || !start_playback(tones, duty_cycle, &waiter)) return false;
        if (xSemaphoreTake(waiter.semaphore, portMAX_DELAY) != pdTRUE) return false;
        return waiter.completed;
    }

    bool pause() {
        return set_paused(true);
    }

    bool resume() {
        return set_paused(false);
    }

    void stop() {
        const unsigned long state = bsp_sys_enter_critical();
        if (player_task == nullptr) {
            bsp_sys_exit_critical(state);
            return;
        }
        finish_waiter(request.waiter, false);
        request = {};
        ++revision;
        active = false;
        paused = false;
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
