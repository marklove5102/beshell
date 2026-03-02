#pragma once

#include "REPLChannel.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace be {
    class REPLSerial: public REPLChannel {
    private:
        TaskHandle_t taskHandle = nullptr ;
        QueueHandle_t uart_queue;
        // QueueHandle_t pkg_queue;

        static void task(void * argv) ;

    public:
        using REPLChannel::REPLChannel ;
        void setup() ;
        void sendData (const char * data, size_t datalen) ;
    } ;
}