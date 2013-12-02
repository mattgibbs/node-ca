#include <node.h>
#include <v8.h>
#include <cadef.h>
#include <stdlib.h>
#include <string>
#include <cstring>
using namespace v8;

uv_async_t async_connect; //This is our async token, we can use this to send messages from other threads to the v8 thread.
uv_loop_t *loop; //This will get set to the default libuv event loop in the init function below.
uv_mutex_t mutexForCallback;
uv_mutex_t mutexForData;
//Persistent<Function> callback;
struct connection_struct {
		uv_async_t *async_handle;
		Persistent<Function> user_callback;
};

/*
v8_subscription_callback is called asynchronously on the main thread after libca_subscription_callback completes.
All the interactions with V8 need to happen in here.
*/
void v8_subscription_callback(uv_async_t *handle, int status) {
	HandleScope scope;
	Local<Object> dataObj = Object::New();

	//Create an array to hold our data.
	uv_mutex_lock(&mutexForData);
	struct event_handler_args *args = (struct event_handler_args *)handle->data;
	Local<Array> array = Array::New(args->count);
	//TODO Write the awful code to turn dbr into javascript objects.  It will be painful.
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

	uv_mutex_unlock(&mutexForData);
	dataObj->Set(String::NewSymbol("data"),array);
	
	uv_mutex_lock(&mutexForCallback);
	Persistent<Function> myCallback = callback;
	uv_mutex_unlock(&mutexForCallback);
	const unsigned argc = 1;
	Local<Value> argv[argc] = { dataObj };
	myCallback->Call(Context::GetCurrent()->Global(), argc, argv);
	return;
}

/*
libca_subscription_callback is called from libca worker threads whenever a subscribed channel gets new data.
This might not be the main thread, so don't call V8 stuff in here!
*/
static void libca_subscription_callback(struct event_handler_args args) {
	if ( args.status != ECA_NORMAL) {
		return;
	}
	uv_async_t *v8CallbackHandle = (uv_async_t *)args.usr;

	//'args' is only valid inside this function.  We need to pass it to v8_subscription_callback.  So we have to copy it.
	struct event_handler_args *event_struct_ptr = (struct event_handler_args *)malloc(sizeof(struct event_handler_args));
	memcpy(event_struct_ptr, &args, sizeof(struct event_handler_args));

	//That last memcpy doesn't get the actual data to which many pointers in the args point.
	unsigned nbytes = dbr_size_n(args.type, args.count);
	void *value_ptr = malloc(nbytes);
	memcpy(value_ptr, args.dbr, nbytes);
	event_struct_ptr->dbr = value_ptr;
	
	//Now we can set our callback handle's 'data' field, so we can get to it inside the v8_subscription_callback.
	uv_mutex_lock(&mutexForData);
	v8CallbackHandle->data = event_struct_ptr;
	uv_mutex_unlock(&mutexForData);
	uv_async_send(v8CallbackHandle);
	return;
}


/*
v8_connection_handler is called asynchronously after libca_connection_handler completes.
It runs the callback that a user supplied in nodeca.connect(PV,callback).
The callback's only argument is a PV object.  The PV object holds the name of the PV, and any control info,
like units and limits.  The PV object also has a 'monitor' method, which can be used to initiate a subscription to the PV.
*/
void v8_connection_handler(uv_async_t *handle, int status) {

	uv_mutex_lock(&mutexForCallback);
	//Persistent<Function> callback = *((Persistent<Function>*) handle->data);
	Persistent<Function> myCallback = callback;
	uv_mutex_unlock(&mutexForCallback);

	uv_mutex_lock(&mutexForData);
	chid *connected_chan = (chid *) handle->data;
	uv_mutex_unlock(&mutexForData);
	
	//chid connected_chan = args.chid;
	const char* pvname = ca_name(*connected_chan);
	free(connected_chan);
	const unsigned argc = 1;
	Local<Value> argv[argc] = { Local<Value>::New(String::New(pvname)) };
	myCallback->Call(Context::GetCurrent()->Global(), argc, argv);
	uv_close((uv_handle_t*) &async_connect,NULL);
	uv_mutex_destroy(&mutexForCallback);
	return;

}

//This function is potentially called from libca worker threads, so we can't use any V8 stuff in here.
static void libca_connection_handler(struct connection_handler_args args) {
	chid *chidptr;
	uv_mutex_lock(&mutexForData);
	chid chan = args.chid;
	chidptr = (chid *)malloc(sizeof(chid));
	memcpy(chidptr, &chan, sizeof(chid)); //Copy the value of chid, since 'args' won't be valid when this function ends
	uv_mutex_unlock(&mutexForData);
	async_connect.data = (void*) chidptr;
	uv_async_send(&async_connect); //Send an async message.  This will call 'v8_connection_handler()', which gets run on the main thread.  The async_connect handle's data field gets assigned a pointer to the newly created channel's id.
	
	//Now set up a subscription for this PV.
	struct connection_struct *puser = (struct connection_struct *)ca_puser(args.chid);
	uv_async_t *asyncConnectionHandler = puser->async_handle;
	Persistent<Function> connection_callback = puser->user_callback;
	unsigned count = ca_element_count(args.chid);
	int eventMask = DBE_VALUE | DBE_ALARM;
	evid *eventID = (evid *)malloc(sizeof(evid));
	ca_create_subscription(DBR_TIME_DOUBLE, count, args.chid, eventMask, libca_subscription_callback, puser, eventID);
	ca_pend_io(1.e-5);
	return;
}

Handle<Value> Connect(const Arguments& args) {
	HandleScope scope;
	if (args.Length() < 1) {
		ThrowException(Exception::TypeError(String::New("Not enough arguments")));
		return scope.Close(Undefined());
	}

	if (!args[0]->IsString()) {
		ThrowException(Exception::TypeError(String::New("PV Must be a string")));
		return scope.Close(Undefined());
	}

	if (args.Length() > 1 && !args[1]->IsFunction()) {
		ThrowException(Exception::TypeError(String::New("Callback must be a function")));
		return scope.Close(Undefined());
	}

	String::AsciiValue inputString(args[0]->ToString());
	std::string pvString = *inputString;

	int result = ca_context_create(ca_enable_preemptive_callback);
	if (result != ECA_NORMAL) {
		ThrowException(Exception::Error(String::New("Couldn't create CA context.")));
	}

	chid chan;
	int status;
	struct connection_struct *cs = (struct connection_struct *) malloc(sizeof(struct connection_struct));

	uv_async_t asyncSubscriptionHandle; //async token to call the v8 subscription handler from secondary threads.
	uv_async_init(uv_default_loop(), &asyncSubscriptionHandle, v8_subscription_callback);
	cs->async_handle = &asyncSubscriptionHandle;	

	if (args.Length() > 1) {
		Persistent<Function> connection_callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
		cs->user_callback = connection_callback;
	}
	
	void *puser = cs; //we can retrieve this pointer with ca_puser() inside the libca connection handler.
	status = ca_create_channel(pvString.c_str(),libca_connection_handler,puser,0,&chan);
	SEVCHK(status, "ca_create_channel()");
	status = ca_pend_io(1.e-5);
	//ca_clear_channel(chan);
	//ca_context_destroy();
	return scope.Close(Number::New(status));
}

void init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("connect"), FunctionTemplate::New(Connect)->GetFunction());
	loop = uv_default_loop();
	uv_mutex_init(&mutexForCallback);
	uv_mutex_init(&mutexForData);
	uv_async_init(loop, &async_connect, connected);
}

NODE_MODULE(nodeca, init)
