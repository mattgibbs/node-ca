#define BUILDING_NODE_EXTENSION
#include <node.h>
#include <iostream>
#include "pvobject.h"

using namespace v8;

Persistent<Function> PVObject::constructor;
//uv_async_t async_connect_handle;
//uv_async_t async_disconnected_handle;
//uv_async_t async_event_handle;
//uv_mutex_t mutex;

PVObject::PVObject(std::string pvName, Persistent<Function> callback) : pv_(pvName) {
  //Set up handles to v8 callbacks, and thread-synchronization stuff.s
  uv_async_init(uv_default_loop(), &async_event_handle, v8_event_handler);
  uv_mutex_init(&mutex);
  connected = false;
	dataCallback = callback;
  void *objectRef = (void *)this;
  int status = ca_create_channel(pv_.c_str(),libca_connection_handler,objectRef,0,&chan);
	SEVCHK(status, "ca_create_channel()");
	ca_pend_io(1.e-5);
}

PVObject::~PVObject() {
  uv_close((uv_handle_t*) &async_event_handle,NULL);
	uv_mutex_destroy(&mutex);
  if (connected) {
    ca_clear_channel(chan);
  }
}

//Init just hooks all of the C++ functions up to V8, and sets up some javascript class stuff.  Nothing but goop.  
void PVObject::Init(Handle<Object> exports) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("PVObject"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  
  tpl->PrototypeTemplate()->Set(String::NewSymbol("channel"),FunctionTemplate::New(getChannel)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("connected"),FunctionTemplate::New(getConnectionStatus)->GetFunction());
  constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("PVObject"), constructor);
}

//This gets hooked up to the javascript constructor, which in turn will call the real C++ constructor, then wrap that object for v8 use.
Handle<Value> PVObject::New(const Arguments& args) {
  HandleScope scope;
  
  if (args.IsConstructCall()) {
    //If we call this as a constructor, like 'new PVObject(blah)'
    //double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
    String::AsciiValue inputString(args[0]->ToString());
    std::string pvString = *inputString;
    Local<Function> cb = Local<Function>::Cast(args[1]);
    if (!cb->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Not a function!")));
      return scope.Close(Undefined());
    }
    
    PVObject *obj = new PVObject(pvString, Persistent<Function>::New(cb));
    obj->Wrap(args.This());
    return args.This();
  } else {
    //If we just call this as a normal function, like 'PVObject(blah)', make it a constructor call.
    std::cout << args.Length() << std::endl;
    const int argc = 2;
    Local<Value> argv[argc] = { args[0], args[1] };
    return scope.Close(constructor->NewInstance(argc, argv));
  }
}

void PVObject::connectionMade() {
  this->connected = true;
  chtype field_type = ca_field_type(chan); //This eventually needs to be replaced to let the user specify the type.
  unsigned count = ca_element_count(chan);
  int eventMask = DBE_VALUE | DBE_ALARM;
  void *objectRef =  (void *)this;
  ca_create_subscription(field_type, count, chan, eventMask, libca_event_handler, objectRef, 0);
}

void PVObject::libca_connection_handler(struct connection_handler_args args) {
  PVObject *self = (PVObject *)ca_puser(args.chid);
  if (args.op == CA_OP_CONN_UP) {
    //Oh good, we have connected.  Set up a subscription to the PV.
    self->connectionMade();
  } else {
    //Handle a disconnection here.
    self->connected = false;
  }
}

void PVObject::libca_event_handler(struct event_handler_args args) {
  if (args.status != ECA_NORMAL) {
    //Handle events with bad status here.  Not really sure what 'bad status' even entails right now...
    return;
  }
  PVObject *self = (PVObject *)args.usr;
	//'args' is only valid inside this function.  We need to pass it to v8_event_handler.  So we have to copy it.
	struct event_handler_args *event_struct_ptr = (struct event_handler_args *)malloc(sizeof(struct event_handler_args));
	memcpy(event_struct_ptr, &args, sizeof(struct event_handler_args));

	//That last memcpy doesn't get the actual data to which many pointers in the args point.
	unsigned nbytes = dbr_size_n(args.type, args.count);
	void *value_ptr = malloc(nbytes); //Freed in v8_event_handler.
	memcpy(value_ptr, args.dbr, nbytes);
	event_struct_ptr->dbr = value_ptr;
  event_struct_ptr->usr = (void *)self;
	//Now we can set our handle's 'data' field, so we can get to it inside the v8_event_handler.
	uv_mutex_lock(&self->mutex);
	self->async_event_handle.data = event_struct_ptr;
	uv_mutex_unlock(&self->mutex);
	uv_async_send(&self->async_event_handle);
	return;  
}

//When a Channel Access subscription event fires, the libca callback will call this on the main thread.
void PVObject::v8_event_handler(uv_async_t *handle, int status) {
	Local<Object> dataObj = Object::New();
	struct event_handler_args *args = (struct event_handler_args *)handle->data;
  PVObject *self = (PVObject *)args->usr;
  uv_mutex_lock(&self->mutex);
	//Create an array to hold our data.
	Local<Array> array = Array::New(args->count);
	//Here is the awful code to turn dbr into javascript objects.  It is quite painful.
	if (args->type == DBR_CHAR) {
		const dbr_char_t *value = (const dbr_char_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_SHORT) {
		const dbr_short_t *value = (const dbr_short_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_ENUM) {
		const dbr_enum_t *value = (const dbr_enum_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::NewFromUnsigned((unsigned short)value[i]));
		}
	} else if (args->type == DBR_LONG) {
		const dbr_long_t *value = (const dbr_long_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_FLOAT) {
		const dbr_float_t *value = (const dbr_float_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_DOUBLE) {
		const dbr_double_t *value = (const dbr_double_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_STRING) {
		const dbr_string_t *value = (const dbr_string_t *)args->dbr;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, String::New((char *)value[i],MAX_STRING_SIZE));
		}
	} else if (args->type == DBR_TIME_STRING) {
		struct dbr_time_string *time_struct = (struct dbr_time_string *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_string_t *value = (const dbr_string_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, String::New((char *)value[i],MAX_STRING_SIZE));
		}
	} else if (args->type == DBR_TIME_SHORT) {
		struct dbr_time_short *time_struct = (struct dbr_time_short *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_short_t *value = (const dbr_short_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_TIME_FLOAT) {
		struct dbr_time_float *time_struct = (struct dbr_time_float *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_float_t *value = (const dbr_float_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_TIME_ENUM) {
		struct dbr_time_enum *time_struct = (struct dbr_time_enum *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_enum_t *value = (const dbr_enum_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::NewFromUnsigned((unsigned short)value[i]));
		}
	} else if (args->type == DBR_TIME_CHAR) {
		struct dbr_time_char *time_struct = (struct dbr_time_char *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_char_t *value = (const dbr_char_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_TIME_LONG) {
		struct dbr_time_long *time_struct = (struct dbr_time_long *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_long_t *value = (const dbr_long_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_TIME_DOUBLE) {
		struct dbr_time_double *time_struct = (struct dbr_time_double *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)time_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)time_struct->severity));
		Local<Object> timestamp = Object::New();
		epicsTimeStamp stamp = time_struct->stamp;
		timestamp->Set(String::NewSymbol("secs"), Integer::New((int)stamp.secPastEpoch));
		timestamp->Set(String::NewSymbol("nsecs"), Integer::New((int)stamp.nsec));
		dataObj->Set(String::NewSymbol("timestamp"), timestamp);
		
		const dbr_double_t *value = (const dbr_double_t *)&time_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_CTRL_STRING) {
		struct dbr_sts_string *ctrl_struct = (struct dbr_sts_string *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		
		const dbr_string_t *value = (const dbr_string_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, String::New((char *)value[i],MAX_STRING_SIZE));
		}
	} else if (args->type == DBR_CTRL_SHORT) {
		struct dbr_ctrl_short *ctrl_struct = (struct dbr_ctrl_short *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		dataObj->Set(String::NewSymbol("units"), String::New(ctrl_struct->units));
		dataObj->Set(String::NewSymbol("upper_disp_limit"), Number::New((double)ctrl_struct->upper_disp_limit));
		dataObj->Set(String::NewSymbol("lower_disp_limit"), Number::New((double)ctrl_struct->lower_disp_limit));
		dataObj->Set(String::NewSymbol("upper_alarm_limit"), Number::New((double)ctrl_struct->upper_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_warning_limit"), Number::New((double)ctrl_struct->upper_warning_limit));
		dataObj->Set(String::NewSymbol("lower_warning_limit"), Number::New((double)ctrl_struct->lower_warning_limit));
		dataObj->Set(String::NewSymbol("lower_alarm_limit"), Number::New((double)ctrl_struct->lower_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_ctrl_limit"), Number::New((double)ctrl_struct->upper_ctrl_limit));
		dataObj->Set(String::NewSymbol("lower_ctrl_limit"), Number::New((double)ctrl_struct->lower_ctrl_limit));
		const dbr_short_t *value = (const dbr_short_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_CTRL_FLOAT) {
		struct dbr_ctrl_float *ctrl_struct = (struct dbr_ctrl_float *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		dataObj->Set(String::NewSymbol("units"), String::New(ctrl_struct->units));
		dataObj->Set(String::NewSymbol("upper_disp_limit"), Number::New((double)ctrl_struct->upper_disp_limit));
		dataObj->Set(String::NewSymbol("lower_disp_limit"), Number::New((double)ctrl_struct->lower_disp_limit));
		dataObj->Set(String::NewSymbol("upper_alarm_limit"), Number::New((double)ctrl_struct->upper_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_warning_limit"), Number::New((double)ctrl_struct->upper_warning_limit));
		dataObj->Set(String::NewSymbol("lower_warning_limit"), Number::New((double)ctrl_struct->lower_warning_limit));
		dataObj->Set(String::NewSymbol("lower_alarm_limit"), Number::New((double)ctrl_struct->lower_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_ctrl_limit"), Number::New((double)ctrl_struct->upper_ctrl_limit));
		dataObj->Set(String::NewSymbol("lower_ctrl_limit"), Number::New((double)ctrl_struct->lower_ctrl_limit));
		const dbr_float_t *value = (const dbr_float_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	} else if (args->type == DBR_CTRL_ENUM) {
		struct dbr_ctrl_enum *ctrl_struct = (struct dbr_ctrl_enum *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		
		Local<Array> enum_strs = Array::New((int)ctrl_struct->no_str);
		for (int strIndex = 0; strIndex < ctrl_struct->no_str; strIndex++) {
			enum_strs->Set(strIndex, String::New((char *)ctrl_struct->strs[strIndex],MAX_ENUM_STRING_SIZE));
		}
		dataObj->Set(String::NewSymbol("enum_strs"), enum_strs);
		
		const dbr_enum_t *value = (const dbr_enum_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::NewFromUnsigned((unsigned short)value[i]));
		}
	} else if (args->type == DBR_CTRL_CHAR) {
		struct dbr_ctrl_char *ctrl_struct = (struct dbr_ctrl_char *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		dataObj->Set(String::NewSymbol("units"), String::New(ctrl_struct->units));
		dataObj->Set(String::NewSymbol("upper_disp_limit"), Number::New((double)ctrl_struct->upper_disp_limit));
		dataObj->Set(String::NewSymbol("lower_disp_limit"), Number::New((double)ctrl_struct->lower_disp_limit));
		dataObj->Set(String::NewSymbol("upper_alarm_limit"), Number::New((double)ctrl_struct->upper_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_warning_limit"), Number::New((double)ctrl_struct->upper_warning_limit));
		dataObj->Set(String::NewSymbol("lower_warning_limit"), Number::New((double)ctrl_struct->lower_warning_limit));
		dataObj->Set(String::NewSymbol("lower_alarm_limit"), Number::New((double)ctrl_struct->lower_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_ctrl_limit"), Number::New((double)ctrl_struct->upper_ctrl_limit));
		dataObj->Set(String::NewSymbol("lower_ctrl_limit"), Number::New((double)ctrl_struct->lower_ctrl_limit));
		const dbr_char_t *value = (const dbr_char_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_CTRL_LONG) {
		struct dbr_ctrl_long *ctrl_struct = (struct dbr_ctrl_long *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		dataObj->Set(String::NewSymbol("units"), String::New(ctrl_struct->units));
		dataObj->Set(String::NewSymbol("upper_disp_limit"), Number::New((double)ctrl_struct->upper_disp_limit));
		dataObj->Set(String::NewSymbol("lower_disp_limit"), Number::New((double)ctrl_struct->lower_disp_limit));
		dataObj->Set(String::NewSymbol("upper_alarm_limit"), Number::New((double)ctrl_struct->upper_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_warning_limit"), Number::New((double)ctrl_struct->upper_warning_limit));
		dataObj->Set(String::NewSymbol("lower_warning_limit"), Number::New((double)ctrl_struct->lower_warning_limit));
		dataObj->Set(String::NewSymbol("lower_alarm_limit"), Number::New((double)ctrl_struct->lower_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_ctrl_limit"), Number::New((double)ctrl_struct->upper_ctrl_limit));
		dataObj->Set(String::NewSymbol("lower_ctrl_limit"), Number::New((double)ctrl_struct->lower_ctrl_limit));
		const dbr_long_t *value = (const dbr_long_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Integer::New((int)value[i]));
		}
	} else if (args->type == DBR_CTRL_DOUBLE) {
		struct dbr_ctrl_double *ctrl_struct = (struct dbr_ctrl_double *)args->dbr;
		dataObj->Set(String::NewSymbol("status"), Integer::New((int)ctrl_struct->status));
		dataObj->Set(String::NewSymbol("severity"), Integer::New((int)ctrl_struct->severity));
		dataObj->Set(String::NewSymbol("units"), String::New(ctrl_struct->units));
		dataObj->Set(String::NewSymbol("upper_disp_limit"), Number::New((double)ctrl_struct->upper_disp_limit));
		dataObj->Set(String::NewSymbol("lower_disp_limit"), Number::New((double)ctrl_struct->lower_disp_limit));
		dataObj->Set(String::NewSymbol("upper_alarm_limit"), Number::New((double)ctrl_struct->upper_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_warning_limit"), Number::New((double)ctrl_struct->upper_warning_limit));
		dataObj->Set(String::NewSymbol("lower_warning_limit"), Number::New((double)ctrl_struct->lower_warning_limit));
		dataObj->Set(String::NewSymbol("lower_alarm_limit"), Number::New((double)ctrl_struct->lower_alarm_limit));
		dataObj->Set(String::NewSymbol("upper_ctrl_limit"), Number::New((double)ctrl_struct->upper_ctrl_limit));
		dataObj->Set(String::NewSymbol("lower_ctrl_limit"), Number::New((double)ctrl_struct->lower_ctrl_limit));
		const dbr_double_t *value = (const dbr_double_t *)&ctrl_struct->value;
		for (long i = 0; i < args->count; i++) {
			array->Set(i, Number::New((double)value[i]));
		}
	}
  
	dataObj->Set(String::NewSymbol("data"),array);
	free(handle->data);
  uv_mutex_unlock(&self->mutex);
	const unsigned argc = 1;
	Local<Value> argv[argc] = { dataObj };
	//Local<Value> argv[argc] = { Local<Value>::New(String::New("new data goes here.")) };
  self->dataCallback->Call(Context::GetCurrent()->Global(), argc, argv);
  
	return;
}

Handle<Value> PVObject::runCallback(Local<Value> data) {
  HandleScope scope;
	const unsigned argc = 1;
	//Local<Value> argv[argc] = { data };
	Local<Value> argv[argc] = { Local<Value>::New(String::New("new data goes here.")) };
  this->dataCallback->Call(Context::GetCurrent()->Global(), argc, argv);
  return scope.Close(Undefined());
}

Handle<Value> PVObject::getChannel(const Arguments& args) {
  HandleScope scope;
  PVObject *obj = ObjectWrap::Unwrap<PVObject>(args.This());
  const char* pv = ca_name(obj->chan);
  return scope.Close(String::New(pv));
}

Handle<Value> PVObject::getConnectionStatus(const Arguments& args) {
  HandleScope scope;
  PVObject *obj = ObjectWrap::Unwrap<PVObject>(args.This());
  return scope.Close(Boolean::New(obj->connected));
}

/*
Handle<Value> PVObject::PlusOne(const Arguments& args) {
  HandleScope scope;
  PVObject *obj = ObjectWrap::Unwrap<PVObject>(args.This());
  obj->value_ += 1;
  return scope.Close(Number::New(obj->value_));
}
*/
