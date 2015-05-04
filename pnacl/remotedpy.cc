
#include "remotedpy.h"

namespace{
   
void *backgroundthread(void *arg){
    RemoteDPYInstance *i = (RemoteDPYInstance *) arg;
    i->Start();   
    return NULL;
}

} // namespace

RemoteDPYInstance::RemoteDPYInstance(PP_Instance instance) 
    : pp::Instance(instance),  
      callback_factory_(this) {   

    if(!pp::TCPSocket::IsAvailable()) {
        PostMessage("TCPSocket not available");
    }

    if(!pp::HostResolver::IsAvailable()) {
        PostMessage("HostResolver not available");
    }
    
    once = false;
    size_ = pp::Size(0,  0);
    pixel_offset_ = 0; 
    image = NULL; 

    pthread_create(&thread_, NULL, backgroundthread, this);        

}

bool RemoteDPYInstance::Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true; 
}

void RemoteDPYInstance::Start() {
  
    resolver_ = pp::HostResolver(this);
    if (resolver_.is_null()) {
        PostMessage("Error creating HostResolver.");
        return;
    }

    int32_t result; 
    
    PP_HostResolver_Hint hint = { PP_NETADDRESS_FAMILY_UNSPECIFIED, 0 };
    result = resolver_.Resolve("pluto", 8192, hint, pp::CompletionCallback::CompletionCallback());
    if(result != PP_OK) {
        PostMessage("Resolve Failed.");
        return;
    }

    socket_ = pp::TCPSocket(this);
    if(socket_.is_null()) {
        PostMessage("Error creating TCPSocket.");
        return;
    }
    
    result = socket_.Connect(resolver_.GetNetAddress(0), pp::CompletionCallback::CompletionCallback());
    if(result != PP_OK) {
        PostMessage("Connect Failed.");
        return; 
    }

    NetworkReadLoop();
}

void RemoteDPYInstance::NetworkReadLoop() {

    while(1) {
    
        int32_t result = socket_.Read(buf_, 32768, pp::CompletionCallback::CompletionCallback());
        if(result < 0) 
            break; 
        
        if(result == 0)
            continue; 

        uint32_t *in = (uint32_t *) buf_;
        for(int p = 0; p < result / sizeof(unsigned int); p++){
 
            // Check for control code. 
            if(in[p] & 0x80000000) {
                int code = (in[p] & 0x7F000000) >> 24;
                
                switch(code) {
                    case 1: 
                        if( size_.width() != (in[p] & 0x00FFF000) >> 12 || size_.height() != (in[p] & 0x0000000FFF) ) { 
                            CreateContext(pp::Size( (in[p] & 0x00FFF000) >> 12, (in[p] & 0x0000000FFF)) ); 
                        }
                        break;
                    case 2: 
                        pixel_offset_ = in[p] & 0x00FFFFFF;
                        break; 
                    case 127: 
                        Paint();
                        break; 
                    default: 
                        fprintf(stderr, "Unknown code: %d\n", code);
                        // TODO: freak out! 
                        break;
                }
            
                // Control code processed, continue. 
                continue; 
            }
    
            // Update the pixel.
            if( (in[p] & 0xFF000000) >> 24 < 1) {
                fprintf(stderr, "BUG PIXLE DUPE: %d\n", (in[p] & 0xFF000000) >> 24);
                
            }
            for( int c = 0; c <  (in[p] & 0xFF000000) >> 24; c++) {
                image[pixel_offset_++] = (in[p] & 0x00FFFFFF) | 0xFF000000;
            }

        }
    }
}

void RemoteDPYInstance::Paint() {
        
    PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
    const bool kDontInitToZero = false;
    pp::ImageData image_data(this, format, size_, kDontInitToZero);
        
    uint32_t* data = static_cast<uint32_t*>(image_data.data());
    if(!data)  
        return;

    uint32_t num_pixels = size_.width() * size_.height();
    for (uint32_t i = 0; i < num_pixels; i++) {
        data[i] = image[i];
    }

    context_.ReplaceContents(&image_data);
    context_.Flush(pp::CompletionCallback::CompletionCallback());
}

void RemoteDPYInstance::DidChangeView(const pp::View& view) {
}

bool RemoteDPYInstance::CreateContext(const pp::Size& new_size) {
    
    if(image) {
        free(image); 
        image = NULL;
    }

    image = (uint32_t *)  malloc(sizeof(uint32_t) * (new_size.width() * new_size.height())); 
    if(!image) 
        return false; 

    const bool kIsAlwaysOpaque = true; 
    context_ = pp::Graphics2D(this, new_size, kIsAlwaysOpaque);

    context_.SetScale(1.0f);
    if (!BindGraphics(context_)) {
        fprintf(stderr, "Unable to bind 2d context!\n");
        context_ = pp::Graphics2D();
        return false;
    }

    size_ = new_size; 
    return true; 
    
}

