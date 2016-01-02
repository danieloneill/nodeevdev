//#include <pthread.h>

namespace Ev {
 extern "C" {
# include <libevdev/libevdev.h>
 }
}
#include <node/uv.h>
#include <nan.h>

#define v8str(a) v8::String::NewFromUtf8(isolate, a)
#define v8num(a) v8::Number::New(isolate, a)

//static pthread_mutex_t st_mutex;

typedef struct _ev_event {
	struct _ev_event	*next;
	Ev::input_event		ev;
} ev_event;

class Evdev : public node::ObjectWrap {
	static v8::Persistent< v8::Function > constructor;

        protected:
		uv_thread_t m_thread;
		uv_async_t m_async;

		ev_event	*m_eventQueue;

		int			m_fd;
		struct Ev::libevdev 	*m_dev;
		Nan::Callback		*m_data;
		bool			m_running;

        public:
                Evdev();
                ~Evdev();

		bool initialise( const v8::FunctionCallbackInfo< v8::Value > &args );
	
		// v8 stuff:
                static void Init(v8::Handle<v8::Object> exports);
                static void New(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void Close(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void Pump(const v8::FunctionCallbackInfo< v8::Value >& args);
		static void _Pump(void *arg);
		static void _Callback( uv_async_t *async );
};

