#pragma once

#include "REPLChannel.hpp"
#include <unistd.h>
#include <sys/types.h>


#ifdef LINUX_PLATFORM


namespace be {
    class REPL ;
    class REPLStdIO: public REPLChannel {
    private:
    
        fd_set readfds;
        struct timeval tv;
        uint8_t buf[10240] ;

    protected:
        void sendData (const char * data, size_t datalen) ;

    public:
        // using REPLChannel::REPLChannel ;
        
        REPLStdIO(REPL *) ;
        void setup () ;
        void loop () ;
    
    } ;
}

#endif