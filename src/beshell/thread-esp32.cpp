#include "thread.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <functional>
#include <utility>

namespace {
    struct RunOnCoreContext {
        std::function<void()> func;
        SemaphoreHandle_t doneSemaphore;
    };

    constexpr char TAG[] = "be.thread";
}

// 函数执行的任务
static void run_lambda_on_core(void *params) {
    auto *ctx = static_cast<RunOnCoreContext*>(params);
    ctx->func();
    xSemaphoreGive(ctx->doneSemaphore);
    vTaskDelete(nullptr);
}

void run_wait_on_core (std::function<void()> func, uint8_t core_id) {
    StaticSemaphore_t doneSemaphoreBuffer;
    SemaphoreHandle_t doneSemaphore = xSemaphoreCreateBinaryStatic(&doneSemaphoreBuffer);

    if(doneSemaphore == nullptr) {
        ESP_LOGE(TAG, "无法创建同步信号量，直接在当前核心执行");
        func();
        return;
    }

    RunOnCoreContext ctx{ std::move(func), doneSemaphore };

    BaseType_t created = xTaskCreatePinnedToCore(
        run_lambda_on_core,
        "lambdaTask",
        4096,
        &ctx,
        5,
        nullptr,
        static_cast<BaseType_t>(core_id)
    );

    if(created == pdPASS) {
        xSemaphoreTake(doneSemaphore, portMAX_DELAY);
    } else {
        ESP_LOGE(TAG, "创建 lambdaTask 失败(%ld)，在当前核心回退执行", static_cast<long>(created));
        ctx.func();
    }

    vSemaphoreDelete(doneSemaphore);
}