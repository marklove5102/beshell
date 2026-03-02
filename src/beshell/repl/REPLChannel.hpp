#pragma once
#include "Protocal.hpp"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

namespace be {
    
    class REPL ;

    class REPLChannel {
    private:
#ifdef ESP_PLATFORM
        SemaphoreHandle_t mutex;
#endif

        FILE * openedFile = nullptr ;

    protected:
        // REPLPkgProcFunc packageHandler = nullptr;
        REPL * repl = nullptr ;
    public:
        bool disableEcho = true ;

        REPLChannel(REPL *) ;
        virtual void sendData (const char * data, size_t datalen) = 0 ;
        void send (const std::string & data, int pkgId=-1, uint8_t cmd=OUTPUT) ;
        virtual void send (const char * data, int datalen=-1, int pkgId=-1, uint8_t cmd=OUTPUT) ;
        virtual void send (Package & pkg) ;
        void sendError (int pkgid, const char * format, ...) ;

        virtual bool mutexTake();
        virtual void mutexGive();
        
        virtual void setup() ;

        friend class REPL ;
    } ;
}