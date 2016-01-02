#ifndef BUILDING_NODE_EXTENSION
# define BUILDING_NODE_EXTENSION
#endif

#include <node/v8.h>
#include <node/node.h>
#include <node/node_object_wrap.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

using namespace v8;

#include "evdev.h"

v8::Persistent< v8::Function > Evdev::constructor;

void Evdev::Init( v8::Handle<v8::Object> exports )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	
	// Prepare constructor template
	v8::Local<FunctionTemplate> tpl = v8::FunctionTemplate::New(isolate, New);
	tpl->SetClassName(v8str("Evdev"));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);

	NODE_SET_PROTOTYPE_METHOD(tpl, "pump", Pump);
	NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
	
	constructor.Reset(isolate, tpl->GetFunction());
	exports->Set(v8str("Evdev"), tpl->GetFunction());
}

void Evdev::New(const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	if( !args.IsConstructCall() )
	{
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		const int argc = 1;
		v8::Local<v8::Value> argv[argc] = { args[0] };
		v8::Local<v8::Function> cons = v8::Local<v8::Function>::New(isolate, constructor);
		args.GetReturnValue().Set(cons->NewInstance(argc, argv));
		return;
	}

	//Invoked as constructor: `new MyObject(...)`
	if (args.Length() < 1) {
		isolate->ThrowException(v8::Exception::TypeError(v8str("Wrong number of arguments.")));
		return;
	}

	Evdev *obj = new Evdev();
	if( !obj->initialise( args ) )
	{
		delete obj;
		return;
	}

	obj->Wrap(args.This());
	args.GetReturnValue().Set(args.This());
}

bool Evdev::initialise( const v8::FunctionCallbackInfo<v8::Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	v8::Local< v8::String > fieldName = args[0]->ToString();
	char fieldChar[ fieldName->Length() + 1 ];
	fieldName->WriteUtf8( fieldChar );
	m_fd = open(fieldChar, O_RDONLY);

	if( m_fd == -1 )
	{
		isolate->ThrowException( v8::Exception::TypeError( v8str("Open failed. (Check path and permissions.)") ) );
		m_dev = NULL;
		return false;
	}

	int rc = Ev::libevdev_new_from_fd(m_fd, &m_dev);
	if( rc < 0 )
	{
		isolate->ThrowException( v8::Exception::TypeError( v8str("Evdev failed initialise this device. (Is it an event device?)") ) );
		m_dev = NULL;
		close( m_fd );
		return false;
	}

	return true;
}

void Evdev::Close( const v8::FunctionCallbackInfo<Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	Evdev* parent = Unwrap<Evdev>(args.Holder());
	parent->m_running = false;
	uv_thread_join( &parent->m_thread );
	uv_close( (uv_handle_t*)&parent->m_async, NULL );
}

void Evdev::Pump( const v8::FunctionCallbackInfo<Value>& args )
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);

	Evdev* parent = Unwrap<Evdev>(args.Holder());
	if( args.Length() > 0 )
	{
		v8::Local<v8::Function> callbackHandle = args[0].As<Function>();
		parent->m_data = new Nan::Callback(callbackHandle);
	}

	uv_async_init( uv_default_loop(), &parent->m_async, _Callback );
	parent->m_async.data = (void*)parent;

	parent->m_running = true;
	uv_thread_create( &parent->m_thread, _Pump, parent );
}

void Evdev::_Pump(void *arg)
{
	Evdev *e = static_cast<Evdev *>(arg);
	struct Ev::input_event ev;


	int rc;
	do {
		rc = Ev::libevdev_next_event(e->m_dev, Ev::LIBEVDEV_READ_FLAG_NORMAL|Ev::LIBEVDEV_READ_FLAG_BLOCKING, &ev);

		// Sync if needed, until done:
		while( rc == Ev::LIBEVDEV_READ_STATUS_SYNC )
			rc = Ev::libevdev_next_event(e->m_dev, Ev::LIBEVDEV_READ_FLAG_SYNC, &ev);

		// Something broke:
		if( rc != Ev::LIBEVDEV_READ_STATUS_SUCCESS )
		{
			v8::Isolate* isolate = v8::Isolate::GetCurrent();
			isolate->ThrowException( v8::Exception::TypeError( v8str("Error received") ) );
			return;
		}

		// Valid packet, if we made it here:
		// Enqueue the event:
		ev_event *n = new ev_event;
		n->next = NULL;
		memcpy( &n->ev, &ev, sizeof(ev) );

		if( !e->m_eventQueue )
			e->m_eventQueue = n;
		else
		{
			ev_event *c = e->m_eventQueue;
			while( c && c->next ) c = c->next;
			c->next = n;
		}

		// Notify the event loop:
		uv_async_send( &e->m_async );

	} while( rc == Ev::LIBEVDEV_READ_STATUS_SUCCESS && e->m_running );

	return;
}

void Evdev::_Callback(uv_async_t *uv)
{
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	v8::HandleScope scope(isolate);
	Evdev *e = static_cast<Evdev *>(uv->data);
	
	ev_event *queue = e->m_eventQueue;
	e->m_eventQueue = NULL;

	if( !e->m_data )
		return;

	// Not the fastest way:
	int length = 0;
	for( ev_event *c=queue; c; c=c->next ) length++;

	// Build result array:
	v8::Local<v8::Array> results = v8::Array::New(isolate, length);

	int idx = 0;
	for( ev_event *c=queue; c; c=c->next )
	{
/*
		printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
			c->ev.time.tv_sec,
			c->ev.time.tv_usec,
			c->ev.type,
			Ev::libevdev_event_type_get_name(c->ev.type),
			c->ev.code,
			Ev::libevdev_event_code_get_name(c->ev.type, c->ev.code),
			c->ev.value);
*/
		v8::Local<v8::Object> time = v8::Object::New(isolate);
		time->Set( v8str("tv_sec"), v8num( c->ev.time.tv_sec ) );
		time->Set( v8str("tv_usec"), v8num( c->ev.time.tv_usec ) );

		v8::Local<v8::Object> container = v8::Object::New(isolate);
		container->Set( v8str("time"), time );
		container->Set( v8str("type"), v8num(c->ev.type) );
		container->Set( v8str("typeName"), v8str( Ev::libevdev_event_type_get_name(c->ev.type) ) );
		container->Set( v8str("code"), v8num(c->ev.code) );
		container->Set( v8str("codeName"), v8str( Ev::libevdev_event_code_get_name(c->ev.type, c->ev.code) ) );
		container->Set( v8str("value"), v8num(c->ev.value) );
		results->Set( idx++, container );
	}

	const int argc = 1;
	v8::Local<v8::Value> argv[argc] = { results };

	Nan::MakeCallback(Nan::GetCurrentContext()->Global(), e->m_data->GetFunction(), argc, argv);

	ev_event *n = queue;
	for( ev_event *c=n; c; c=n )
	{
		n = c->next;
		delete c;
	}
}

Evdev::Evdev() :
	m_eventQueue(NULL),
	m_fd(0),
	m_dev(NULL),
	m_data(NULL),
	m_running(false)
{
}

Evdev::~Evdev()
{
	if( m_running )
	{
		m_running = false;
		uv_thread_join( &m_thread );
		uv_close( (uv_handle_t*)&m_async, NULL );
	}

	if( m_dev )
	{
		Ev::libevdev_free( m_dev );
		close( m_fd );
		delete m_data;
	}
}

// "Actual" node init:
static void init(v8::Handle<v8::Object> exports) {
	Evdev::Init(exports);
}

// @see http://github.com/ry/node/blob/v0.2.0/src/node.h#L101
NODE_MODULE(nodeevdev, init);
