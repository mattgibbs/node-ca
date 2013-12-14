#include <node.h>
#include <v8.h>
#include <cadef.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include "pvobject.h"

using namespace v8;

void init(Handle<Object> exports) {
  PVObject::Init(exports);
  ca_context_create(ca_enable_preemptive_callback);
}

NODE_MODULE(nodeca, init)
