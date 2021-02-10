#include <Windows.h>
#include <napi.h>
#include <time.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

Napi::ThreadSafeFunction tsfn;
std::thread nativeThread;

const UINT STOP_MESSAGE = WM_USER + 1;

// variable to store the HANDLE to the hook. Don't declare it anywhere else then globally
// or you will get problems since every function uses this variable.
HHOOK _hook;

// This struct contains the data received by the hook callback. As you see in the callback function
// it contains the thing you will need: vkCode = virtual key code.
KBDLLHOOKSTRUCT kbdStruct;

std::string ConvertKeyCodeToString(int key_stroke);

// Trigger the JS callback when a key is pressed
void Start(const Napi::CallbackInfo& info) {
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
                if (wParam == WM_KEYDOWN || wParam == WM_KEYUP) {
                    // lParam is the pointer to the struct containing the data needed, so cast and
                    // assign it to kdbStruct.
                    kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);

                    napi_status status =
                      tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
                          jsCallback.Call(
                            {Napi::String::New(env, ConvertKeyCodeToString(kbdStruct.vkCode)),
                             Napi::Boolean::New(env, wParam == WM_KEYUP)});
                      });
                    if (status != napi_ok) {
                        std::cout << "Failed to execute BlockingCall!" << std::endl;
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
            std::cout << "Failed to install hook!" << std::endl;
        }

        // Create a message loop
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
            if (bRet == -1) {
                // handle the error and possibly exit
            } else if (msg.message == STOP_MESSAGE) {
                PostQuitMessage(0);
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    });
}

void Stop(const Napi::CallbackInfo& info) {
    if (tsfn) {
        // Terminate native thread
        PostThreadMessageA(GetThreadId(nativeThread.native_handle()), STOP_MESSAGE, NULL, NULL);

        // Release the TSFN
        napi_status status = tsfn.Release();
        if (status != napi_ok) {
            std::cout << "Failed to release the TSFN!" << std::endl;
        }

        tsfn = NULL;
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

std::string ConvertKeyCodeToString(int key_stroke) {
    if ((key_stroke == 1) || (key_stroke == 2)) {
        return "";  // ignore mouse clicks
    }

    std::stringstream output;
    HWND foreground = GetForegroundWindow();
    DWORD threadID;
    HKL layout = NULL;

    if (foreground) {
        // get keyboard layout of the thread
        threadID = GetWindowThreadProcessId(foreground, NULL);
        layout = GetKeyboardLayout(threadID);
    }

    if (key_stroke == VK_BACK) {
        output << "Backspace";
    } else if (key_stroke == VK_RETURN) {
        output << "Enter";
    } else if (key_stroke == VK_SPACE) {
        output << "Space";
    } else if (key_stroke == VK_TAB) {
        output << "Tab";
    } else if (key_stroke == VK_SHIFT || key_stroke == VK_LSHIFT || key_stroke == VK_RSHIFT) {
        output << "Shift";
    } else if (key_stroke == VK_CONTROL || key_stroke == VK_LCONTROL || key_stroke == VK_RCONTROL) {
        output << "Control";
    } else if (key_stroke == VK_ESCAPE) {
        output << "Escape";
    } else if (key_stroke == VK_END) {
        output << "End";
    } else if (key_stroke == VK_HOME) {
        output << "Home";
    } else if (key_stroke == VK_LEFT) {
        output << "ArrowLeft";
    } else if (key_stroke == VK_UP) {
        output << "ArrowUp";
    } else if (key_stroke == VK_RIGHT) {
        output << "ArrowRight";
    } else if (key_stroke == VK_DOWN) {
        output << "ArrowDown";
    } else if (key_stroke == 190 || key_stroke == 110) {
        output << ".";
    } else if (key_stroke == 189 || key_stroke == 109) {
        output << "-";
    } else if (key_stroke == 20) {
        output << "CapsLock";
    } else {
        // map virtual key according to keyboard layout
        char key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);

        output << char(key);
    }

    return output.str();
}

NODE_API_MODULE(push_to_talk, Init)
