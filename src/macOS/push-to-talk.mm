#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <napi.h>

#include <iostream>
#include <map>
#include <string>
#include <thread>

Napi::ThreadSafeFunction tsfn;
std::thread nativeThread;

BOOL shouldNativeThreadKeepRunning;
std::map<int, bool> modifiers;  // to check modifiers state (up or down)

void ReleaseTSFN();
std::string ConvertKeyCodeToString(int key_stroke);

// Trigger the JS callback when a key is pressed
void Start(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Stop if already running
    ReleaseTSFN();

    shouldNativeThreadKeepRunning = YES;

    // Create a ThreadSafeFunction
    tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
      "Keyboard Events",             // Name
      0,                             // Unlimited queue
      1,                             // Only one thread will use this initially
      [](Napi::Env) {                // Finalizer used to clean threads up
          std::cout << "trying to clean up native thread" << std::endl;
          shouldNativeThreadKeepRunning = NO;
          nativeThread.join();
          std::cout << "native thread joined" << std::endl;
      });

    nativeThread = std::thread([] {
        modifiers.clear();

        auto CGEventCallback = [](CGEventTapProxy proxy, CGEventType type, CGEventRef event,
                                  void* refcon) {
            if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
                return event;
            }

            CGKeyCode keyCode =
              (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

            // get key direction
            bool isKeyUp = false;
            if (type == kCGEventKeyUp) {
                isKeyUp = true;
            } else if (type == kCGEventFlagsChanged) {
                std::map<int, bool>::iterator iter = modifiers.find(keyCode);
                if (iter == modifiers.end()) {
                    // not found
                    modifiers[keyCode] = true;
                } else {
                    // found
                    modifiers.erase(keyCode);
                    isKeyUp = true;
                }
            }

            napi_status status = tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call({Napi::String::New(env, ConvertKeyCodeToString(keyCode)),
                                 Napi::Boolean::New(env, isKeyUp)});
            });
            if (status != napi_ok) {
                std::cout << "Failed to execute BlockingCall!" << std::endl;
            }

            return event;
        };

        CGEventMask eventMask = (CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                                 CGEventMaskBit(kCGEventFlagsChanged));
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

        NSRunLoop* theRL = [NSRunLoop currentRunLoop];
        while (shouldNativeThreadKeepRunning &&
               [theRL runMode:NSDefaultRunLoopMode
                   beforeDate:[NSDate dateWithTimeInterval:1.0 sinceDate:[NSDate date]]])
            ;
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

// Try to match web values
// https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
std::string ConvertKeyCodeToString(int key_stroke) {
    switch ((int)key_stroke) {
        case kVK_Option:
        case kVK_RightOption:
            return "Alt";
        default:
            return "unknown";
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(push_to_talk, Init)
