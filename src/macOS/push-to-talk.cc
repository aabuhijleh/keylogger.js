#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <napi.h>

#include <iostream>
#include <string>
#include <thread>

Napi::ThreadSafeFunction tsfn;
std::thread nativeThread;

void ReleaseTSFN();

// Trigger the JS callback when a key is pressed
void Start(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Stop if already running
    ReleaseTSFN();

    // Create a ThreadSafeFunction
    tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
      "Keyboard Events",             // Name
      0,                             // Unlimited queue
      1,                             // Only one thread will use this initially
      [](Napi::Env) {                // Finalizer used to clean threads up
          std::cout << "trying to clean up native thread" << std::endl;
          // TODO: somehow signal nativeThread to exit
          nativeThread.join();
          std::cout << "native thread joined" << std::endl;
      });

    nativeThread = std::thread([] {
        auto CGEventCallback = [](CGEventTapProxy proxy, CGEventType type, CGEventRef event,
                                  void* refcon) {
            if (type != kCGEventKeyDown && type != kCGEventFlagsChanged && type != kCGEventKeyUp) {
                return event;
            }

            CGKeyCode keyCode =
              (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

            std::cout << "\nKEYDOWN - " << keyCode << std::endl;

            return event;
        };

        CGEventMask eventMask =
          (CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventFlagsChanged));
        CFMachPortRef eventTap =
          CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                           eventMask, CGEventCallback, NULL);

        if (!eventTap) {
            std::cout << "Failed to create event tap" << std::endl;
            return;
        }

        CFRunLoopSourceRef runLoopSource =
          CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
        CGEventTapEnable(eventTap, true);

        CFRunLoopRun();
    });
}

void Stop(const Napi::CallbackInfo& info) {
    ReleaseTSFN();
}

void ReleaseTSFN() {
    if (tsfn) {
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

NODE_API_MODULE(push_to_talk, Init)
