#include <Windows.h>
#include <napi.h>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>

// One global TSFN bridging the native hook thread and the JS thread. All
// access is synchronized through g_tsfnMutex: the hook callback enqueues
// events, Start() replaces it, and ReleaseTSFN() releases it.
Napi::ThreadSafeFunction g_tsfn;
std::mutex g_tsfnMutex;

// Events dropped because the bounded TSFN queue was full. The N-API queue
// cannot evict the oldest entry, so the incoming event is dropped and counted
// here instead of stalling keyboard input.
std::atomic<uint64_t> g_droppedEvents{0};

// Data structure representing our thread-safe function context.
struct TsfnContext {
    TsfnContext(Napi::Env env) {
    }

    std::thread nativeThread;
    HHOOK hook = NULL;
};

// A single keyboard event, copied by value out of KBDLLHOOKSTRUCT on the hook
// thread and heap-allocated so it survives until the JS thread dequeues it.
struct KeyEventData {
    DWORD vkCode;
    bool extended;
    bool up;
    bool repeat;
};

// Custom message posted to the native thread to signal it to quit.
const UINT STOP_MESSAGE = WM_USER + 1;

std::string GetLastErrorAsString();
void ReleaseTSFN();
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void NativeThreadMain(TsfnContext *context, std::promise<void> hookInstalled);
void DispatchKeyEvent(Napi::Env env, Napi::Function jsCallback, KeyEventData *event);
std::string TranslateCharacter(DWORD vkCode);

// The thread-safe function finalizer callback. This callback executes
// at destruction of thread-safe function, taking as arguments the finalizer
// data and threadsafe-function context.
void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context);

// Called from JS with a dispatch function as an argument. Installs a low-level
// keyboard hook on a dedicated native thread and reports every keyboard event
// to the dispatch function asynchronously via the TSFN, as one object:
// { keyCode, extended, state, repeat, character }.
void Start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "keylogger: start(dispatch) requires a function argument")
          .ThrowAsJavaScriptException();
        return;
    }

    // Stop a previous listener first rather than corrupting its state. The JS
    // wrapper already prevents calling start() while listening.
    ReleaseTSFN();

    // Construct context data
    auto contextData = new TsfnContext(env);

    // Create a ThreadSafeFunction
    {
        std::lock_guard<std::mutex> lock(g_tsfnMutex);
        g_tsfn = Napi::ThreadSafeFunction::New(
          env,
          info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
          "Keyboard Events",             // Name
          1024,                          // Bounded queue; excess events are dropped
          1,                             // Only one thread will use this initially
          contextData,                   // Context that can be accessed by Finalizer
          FinalizerCallback,             // Finalizer used to clean threads up
          (void *)nullptr                // Finalizer data
        );
    }

    std::promise<void> hookPromise;
    std::future<void> hookFuture = hookPromise.get_future();

    // Create a native thread with its own message loop which is required to
    // attach low level keyboard hooks in order not to block the main thread
    try {
        contextData->nativeThread =
          std::thread(NativeThreadMain, contextData, std::move(hookPromise));
    } catch (const std::system_error &e) {
        ReleaseTSFN();  // the finalizer deletes contextData
        Napi::Error::New(env, std::string("keylogger: failed to start hook thread: ") + e.what())
          .ThrowAsJavaScriptException();
        return;
    }

    // The hook is installed on the native thread; wait for the outcome so a
    // failed install surfaces as a JS exception thrown by start().
    try {
        hookFuture.get();
    } catch (const std::exception &e) {
        // The native thread already exited after failing to install the hook.
        if (contextData->nativeThread.joinable()) {
            contextData->nativeThread.join();
        }
        ReleaseTSFN();  // the finalizer deletes contextData
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return;
    }
}

// Called from JS to release the TSFN and stop listening to keyboard events.
// Safe when start() was never called and safe to call twice.
void Stop(const Napi::CallbackInfo &info) {
    ReleaseTSFN();
}

// Release the TSFN. Idempotent: does nothing when no listener is active. The
// finalizer (see FinalizerCallback) tears down the native thread and context.
void ReleaseTSFN() {
    std::lock_guard<std::mutex> lock(g_tsfnMutex);
    if (g_tsfn) {
        napi_status status = g_tsfn.Release();
        (void)status;  // nothing actionable here; the finalizer still runs
        g_tsfn = nullptr;
    }
}

// Body of the dedicated native thread. Installs the hook, reports the outcome
// to Start() through the promise, then runs the message loop that
// WH_KEYBOARD_LL requires until STOP_MESSAGE arrives. Unhooks before exiting —
// Microsoft requires this and 0.0.4 leaked the hook.
void NativeThreadMain(TsfnContext *context, std::promise<void> hookInstalled) {
    // Set the hook and set it to use the callback function above.
    // WH_KEYBOARD_LL sets a low level keyboard hook. The last 2 parameters are
    // NULL, 0 because the callback function is in the same thread as the
    // function that sets and releases the hook.
    context->hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (context->hook == NULL) {
        std::string message =
          "keylogger: SetWindowsHookEx(WH_KEYBOARD_LL) failed: " + GetLastErrorAsString();
        hookInstalled.set_exception(std::make_exception_ptr(std::runtime_error(message)));
        return;
    }
    hookInstalled.set_value();

    // Create a message loop
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            break;  // GetMessage failed; fall through to unhook and exit
        } else if (msg.message == STOP_MESSAGE) {
            break;
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    UnhookWindowsHookEx(context->hook);
    context->hook = NULL;
}

// Runs on the hook thread. Must return almost immediately: Microsoft silently
// removes hooks that exceed LowLevelHooksTimeout, so there is no translation,
// no I/O, and no blocking here — only a by-value copy of the event (the old
// global KBDLLHOOKSTRUCT raced with the next event) and a non-blocking
// enqueue.
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        const KBDLLHOOKSTRUCT *kbd = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);

        auto *event = new KeyEventData();
        event->vkCode = kbd->vkCode;
        event->extended = (kbd->flags & LLKHF_EXTENDED) != 0;
        event->up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

        // WH_KEYBOARD_LL carries no repeat bit; a key down for a vkCode that is
        // already down is an auto-repeat. The set is thread_local, so it lives
        // on the hook thread and is discarded when the thread exits.
        static thread_local std::unordered_set<DWORD> downKeys;
        if (event->up) {
            downKeys.erase(event->vkCode);
            event->repeat = false;
        } else {
            event->repeat = !downKeys.insert(event->vkCode).second;
        }

        std::lock_guard<std::mutex> lock(g_tsfnMutex);
        if (g_tsfn) {
            napi_status status = g_tsfn.NonBlockingCall(event, DispatchKeyEvent);
            if (status != napi_ok) {
                // napi_queue_full: the bounded queue cannot evict the oldest
                // entry, so drop the incoming event and count it. Any other
                // error means the TSFN is closing; drop silently. Either way,
                // never stall keyboard input.
                if (status == napi_queue_full) {
                    ++g_droppedEvents;
                }
                delete event;
            }
        } else {
            delete event;
        }
    }

    // Call the next hook in the hook chain. This is necessary or the hook chain
    // breaks and the hook stops. The first parameter is ignored and may be NULL.
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Runs on the JS thread once per queued event. Builds the event object,
// translates the character (never do this on the hook thread), and invokes
// the dispatch function.
void DispatchKeyEvent(Napi::Env env, Napi::Function jsCallback, KeyEventData *event) {
    if (event == nullptr) {
        return;
    }
    if (env == nullptr) {
        // The TSFN was destroyed with items still queued; nothing to call.
        delete event;
        return;
    }

    Napi::Object raw = Napi::Object::New(env);
    raw.Set("keyCode", Napi::Number::New(env, event->vkCode));
    raw.Set("extended", Napi::Boolean::New(env, event->extended));
    raw.Set("state", Napi::String::New(env, event->up ? "up" : "down"));
    raw.Set("repeat", Napi::Boolean::New(env, event->repeat));
    raw.Set("character", Napi::String::New(env, TranslateCharacter(event->vkCode)));
    delete event;

    jsCallback.Call({raw});
    if (env.IsExceptionPending()) {
        // A JS exception thrown by the dispatch function must not escape into
        // N-API internals; swallow it.
        env.GetAndClearPendingException();
    }
}

// Maps a virtual-key code to the keyboard layout's unshifted character, or ""
// when the key has no printable translation. 1.0.0 limitation: there is no
// shift-state translation (shift+1 reports "1", not "!"), and characters
// outside the ANSI low byte are not produced.
std::string TranslateCharacter(DWORD vkCode) {
    UINT result = MapVirtualKeyExA(vkCode, MAPVK_VK_TO_CHAR, GetKeyboardLayout(0));
    if (result == 0 || (result & 0x80000000u) != 0) {
        return "";  // no translation, or a dead key (high bit set)
    }
    char c = static_cast<char>(result & 0xFF);
    if (std::iscntrl(static_cast<unsigned char>(c))) {
        return "";
    }
    return std::string(1, c);
}

// Returns the last Win32 error, in string format. Returns an empty string if
// there is no error.
std::string GetLastErrorAsString() {
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) {
        return std::string();  // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the
    // message for us (because we don't yet know how long the message string will
    // be).
    size_t size = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0,
      NULL);

    // Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

// The TSFN finalizer. Runs on the JS thread when the TSFN is released: posts
// the quit message to the native thread's message loop, joins the thread (the
// thread itself calls UnhookWindowsHookEx before exiting, as Microsoft
// requires), and deletes the context.
void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context) {
    if (context->nativeThread.joinable()) {
        DWORD threadId = GetThreadId(context->nativeThread.native_handle());
        if (threadId != 0) {
            PostThreadMessageA(threadId, STOP_MESSAGE, NULL, NULL);
        }
        context->nativeThread.join();
    }

    delete context;
}

// Declare JS functions and map them to native functions
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(keylogger, Init)
