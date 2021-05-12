#include <Windows.h>
#include <napi.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// The TSFN is used to bridge the C++ world and the JS world
Napi::ThreadSafeFunction tsfn;

// Data structure representing our thread-safe function context.
struct TsfnContext {
    TsfnContext(Napi::Env env){};

    std::thread nativeThread;
};

// custom message sent to the native thread to signal it to quit
const UINT STOP_MESSAGE = WM_USER + 1;

// variable to store the HANDLE to the hook. Don't declare it anywhere else then
// globally or you will get problems since every function uses this variable.
HHOOK _hook;

// This struct contains the data received by the hook callback. As you see in
// the callback function it contains the thing you will need: vkCode = virtual
// key code.
KBDLLHOOKSTRUCT kbdStruct;

void ReleaseTSFN();
std::string ConvertKeyCodeToString(int key_stroke);
std::string GetLastErrorAsString();

// The thread-safe function finalizer callback. This callback executes
// at destruction of thread-safe function, taking as arguments the finalizer
// data and threadsafe-function context.
void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context);

// Called from JS with a callback as an argument. It should call the JS callback
// from inside the native thread when reciving a keyboard input event
// jsCallback: (key: string, isKeyUp: boolean) => void
void Start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    // Stop if already running
    ReleaseTSFN();

    // Construct context data
    auto contextData = new TsfnContext(env);

    // Create a ThreadSafeFunction
    tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
      "Keyboard Events",             // Name
      0,                             // Unlimited queue
      1,                             // Only one thread will use this initially
      contextData,                   // Context that can be accessed by Finalizer
      FinalizerCallback,             // Finalizer used to clean threads up
      (void *)nullptr                // Finalizer data
    );

    // Create a native thread with its own message loop which is required to
    // attach low level keyboard hooks in order not to block the main thread
    contextData->nativeThread = std::thread([] {
        // This is the callback function. Consider it the event that is raised when,
        // in this case, a key is pressed or released.
        static auto HookCallback = [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            try {
                if (nCode >= 0 && tsfn) {
                    // the action is valid: HC_ACTION and tsfn is not released.

                    // lParam is the pointer to the struct containing the data needed,
                    // so cast and assign it to kdbStruct.
                    kbdStruct = *((KBDLLHOOKSTRUCT *)lParam);

                    // call the JS callback with the key input value and type
                    napi_status status =
                      tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
                          jsCallback.Call(
                            {Napi::String::New(env, ConvertKeyCodeToString(kbdStruct.vkCode)),
                             Napi::Boolean::New(env, wParam == WM_KEYUP || wParam == WM_SYSKEYUP)});
                      });
                    if (status != napi_ok) {
                        std::cerr << "Failed to execute BlockingCall!" << std::endl;
                    }
                }
            } catch (...) {
                std::cerr << "Something went wrong while handling the key event" << std::endl;
            }

            // call the next hook in the hook chain. This is nessecary or your hook
            // chain will break and the hook stops
            return CallNextHookEx(_hook, nCode, wParam, lParam);
        };

        // Set the hook and set it to use the callback function above
        // WH_KEYBOARD_LL means it will set a low level keyboard hook. More
        // information about it at MSDN. The last 2 parameters are NULL, 0 because
        // the callback function is in the same thread and window as the function
        // that sets and releases the hook.
        if (!(_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
            std::cerr << "Failed to install hook!" << std::endl;
        }

        // Create a message loop
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
            if (bRet == -1) {
                // handle the error and possibly exit
                std::cerr << "Some error occurred in the message loop" << std::endl;
            } else if (msg.message == STOP_MESSAGE) {
                PostQuitMessage(0);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    });
}

// Called from JS to release the TSFN and stop listening to keyboard events
void Stop(const Napi::CallbackInfo &info) {
    ReleaseTSFN();
}

// Release the TSFN
void ReleaseTSFN() {
    if (tsfn) {
        napi_status status = tsfn.Release();
        if (status != napi_ok) {
            std::cerr << "Failed to release the TSFN!" << std::endl;
        }
        tsfn = NULL;
    }
}

// Convert vkeyCode to string that matches these browser values
// https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
std::string ConvertKeyCodeToString(int key_stroke) {
    if ((key_stroke == 1) || (key_stroke == 2)) {
        return "";  // ignore mouse clicks
    }

    std::stringstream output;

    switch (key_stroke) {
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
            output << "Alt";
            break;
        case VK_LWIN:
        case VK_RWIN:
            output << "Meta";
            break;
        case VK_BACK:
            output << "Backspace";
            break;
        case VK_RETURN:
            output << "Enter";
            break;
        case VK_SPACE:
            output << "Spacebar";
            break;
        case VK_TAB:
            output << "Tab";
            break;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
            output << "Shift";
            break;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            output << "Control";
            break;
        case VK_ESCAPE:
            output << "Escape";
            break;
        case VK_END:
            output << "End";
            break;
        case VK_HOME:
            output << "Home";
            break;
        case VK_LEFT:
            output << "ArrowLeft";
            break;
        case VK_UP:
            output << "ArrowUp";
            break;
        case VK_RIGHT:
            output << "ArrowRight";
            break;
        case VK_DOWN:
            output << "ArrowDown";
            break;
        case VK_CAPITAL:
            output << "CapsLock";
            break;
        case VK_PRIOR:
            output << "PageUp";
            break;
        case VK_NEXT:
            output << "PageDown";
            break;
        case VK_DELETE:
            output << "Delete";
            break;
        case VK_INSERT:
            output << "Insert";
            break;
        case VK_SNAPSHOT:
            output << "PrintScreen";
            break;
        case 190:
        case 110:
            output << ".";
            break;
        case 189:
        case 109:
            output << "-";
            break;
        default:
            if (key_stroke >= VK_F1 && key_stroke <= VK_F20) {
                output << "F" << (key_stroke - VK_F1 + 1);
            } else {
                // map virtual key according to keyboard layout
                char key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, GetKeyboardLayout(0));
                output << char(key);
            }
    }

    return output.str();
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

void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context) {
    DWORD threadId = GetThreadId(context->nativeThread.native_handle());
    if (threadId == 0) {
        std::cerr << "GetThreadId failed: " << GetLastErrorAsString() << std::endl;
    }

    PostThreadMessageA(threadId, STOP_MESSAGE, NULL, NULL);

    if (context->nativeThread.joinable()) {
        context->nativeThread.join();
    } else {
        std::cerr << "Failed to join nativeThread!" << std::endl;
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
