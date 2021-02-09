#include <Windows.h>
#include <napi.h>
#include <time.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

Napi::ThreadSafeFunction tsfn;
std::thread nativeThread;

// Create a native callback function to be invoked by the TSFN
auto callback = [](Napi::Env env, Napi::Function jsCallback, int* value) {
    // Call the JS callback
    jsCallback.Call({Napi::Number::New(env, *value)});

    // We're finished with the data.
    delete value;
};

// variable to store the HANDLE to the hook. Don't declare it anywhere else then globally
// or you will get problems since every function uses this variable.
HHOOK _hook;

// This struct contains the data received by the hook callback. As you see in the callback function
// it contains the thing you will need: vkCode = virtual key code.
KBDLLHOOKSTRUCT kbdStruct;

// Trigger the JS callback when a key is pressed
void Start(const Napi::CallbackInfo& info) {
    std::cout << "Start is called" << std::endl;

    Napi::Env env = info.Env();

    // Create a ThreadSafeFunction
    tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
      "Keyboard Events",             // Name
      0,                             // Unlimited queue
      1,                             // Only one thread will use this initially
      [](Napi::Env) {                // Finalizer used to clean threads up
          nativeThread.join();
      });

    nativeThread = std::thread([] {
        // This is the callback function. Consider it the event that is raised when, in this case,
        // a key is pressed.
        static auto HookCallback = [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (nCode >= 0) {
                // the action is valid: HC_ACTION.
                if (wParam == WM_KEYDOWN) {
                    // lParam is the pointer to the struct containing the data needed, so cast and
                    // assign it to kdbStruct.
                    kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);

                    // Send (kbdStruct.vkCode) to JS world via "start" function callback parameter
                    int* value = new int(kbdStruct.vkCode);
                    napi_status status = tsfn.BlockingCall(value, callback);
                    if (status != napi_ok) {
                        std::cout << "BlockingCall is not ok" << std::endl;
                    }
                }
            }

            // call the next hook in the hook chain. This is nessecary or your hook chain will
            // break and the hook stops
            return CallNextHookEx(_hook, nCode, wParam, lParam);
        };

        // Set the hook and set it to use the callback function above
        // WH_KEYBOARD_LL means it will set a low level keyboard hook. More information about it at
        // MSDN. The last 2 parameters are NULL, 0 because the callback function is in the same
        // thread and window as the function that sets and releases the hook.
        if (!(_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
            LPCSTR a = "Failed to install hook!";
            LPCSTR b = "Error";
            MessageBox(NULL, a, b, MB_ICONERROR);
        }

        // Create a message loop
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
            if (bRet == -1) {
                // handle the error and possibly exit
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    });
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    return exports;
}

NODE_API_MODULE(push_to_talk, Init)
