#ifndef PVOBJECT_H
#define PVOBJECT_H

#include <node.h>
#include <cadef.h>
#include <stdlib.h>
#include <string>
#include <cstring>

class PVObject : public node::ObjectWrap {
  public:
    static void Init(v8::Handle<v8::Object> exports);
    bool connected;
    uv_async_t async_event_handle;
    uv_mutex_t mutex;
    v8::Persistent<v8::Function> dataCallback;
    void connectionMade();
    chid chan;
  private:
    explicit PVObject(std::string pvName, v8::Persistent<v8::Function> callback);
    ~PVObject();
    static void v8_event_handler(uv_async_t *handle, int status);
    static void libca_event_handler(struct event_handler_args args);
    static void libca_connection_handler(struct connection_handler_args args);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> getChannel(const v8::Arguments& args);
    static v8::Handle<v8::Value> getConnectionStatus(const v8::Arguments& args);
    static v8::Persistent<v8::Function> constructor;
    std::string pv_;
};

#endif
