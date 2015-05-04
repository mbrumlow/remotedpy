#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/host_resolver.h"
#include "ppapi/utility/completion_callback_factory.h"

class RemoteDPYInstance : public pp::Instance {
    public:
        RemoteDPYInstance(PP_Instance instance);
        ~RemoteDPYInstance() { if(image) { free(image); }  }

        virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
        virtual void DidChangeView(const pp::View& view);
        void Start();

    private:

        bool CreateContext(const pp::Size& new_size);
        void Paint();
        void NetworkReadLoop();

        pp::CompletionCallbackFactory<RemoteDPYInstance> callback_factory_;

        pp::Graphics2D context_;
        pp::Graphics2D flush_context_;
        pp::Size size_;

        pp::HostResolver resolver_;
        pp::TCPSocket socket_;

        char buf_[65536];

        uint32_t *image;
        int pixel_offset_;

        bool once;
        pthread_t thread_;
        //bool was_fullscreen_;
       // float device_scale_;
};


class RemoteDPYModule : public pp::Module {
    public:
        RemoteDPYModule() : pp::Module() {}
        virtual ~RemoteDPYModule() {}

        virtual pp::Instance* CreateInstance(PP_Instance instance) {
            return new RemoteDPYInstance(instance);
        }
};

namespace pp {
Module* CreateModule() { return new RemoteDPYModule(); }
} // namespace pp

