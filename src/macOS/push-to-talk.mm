#import <Cocoa/Cocoa.h>

#include <Carbon/Carbon.h>
#include <napi.h>

#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "./keyboard_codes.h"
#include "./string_conversion.h"

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

            if (!tsfn)
                return event;

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

std::string ConvertKeyCodeToText(int mac_key_code) {
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    CFDataRef layout_data =
      static_cast<CFDataRef>((TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData)));
    if (!layout_data) {
        // TISGetInputSourceProperty returns null with  Japanese keyboard layout.
        // Using TISCopyCurrentKeyboardLayoutInputSource to fix NULL return.
        source = TISCopyCurrentKeyboardLayoutInputSource();
        layout_data = static_cast<CFDataRef>(
          (TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData)));
        if (!layout_data) {
            // https://developer.apple.com/library/mac/documentation/TextFonts/Reference/TextInputSourcesReference/#//apple_ref/c/func/TISGetInputSourceProperty
            return std::string();
        }
    }

    const UCKeyboardLayout* keyboardLayout =
      reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data));

    int mac_modifiers = 0;

    // Convert EventRecord modifiers to format UCKeyTranslate accepts. See docs
    // on UCKeyTranslate for more info.
    UInt32 modifier_key_state = (mac_modifiers >> 8) & 0xFF;

    UInt32 dead_key_state = 0;
    UniCharCount char_count = 0;
    UniChar character = 0;
    OSStatus status = UCKeyTranslate(
      keyboardLayout, static_cast<UInt16>(mac_key_code), kUCKeyActionDown, modifier_key_state,
      LMGetKbdLast(), kUCKeyTranslateNoDeadKeysBit, &dead_key_state, 1, &char_count, &character);

    bool isDeadKey = false;
    if (status == noErr && char_count == 0 && dead_key_state != 0) {
        isDeadKey = true;
        status = UCKeyTranslate(keyboardLayout, static_cast<UInt16>(mac_key_code), kUCKeyActionDown,
                                modifier_key_state, LMGetKbdLast(), kUCKeyTranslateNoDeadKeysBit,
                                &dead_key_state, 1, &char_count, &character);
    }

    if (status == noErr && char_count == 1 && !std::iscntrl(character)) {
        wchar_t value = character;
        return vscode_keyboard::UTF16toUTF8(&value, 1);
    }
    return std::string();
}

// Try to match web values
// https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
std::string ConvertKeyCodeToString(int key_stroke) {
    switch ((int)key_stroke) {
        case kVK_Option:
        case kVK_RightOption:
            return "Alt";
        case kVK_CapsLock:
            return "CapsLock";
        case kVK_Control:
        case kVK_RightControl:
            return "Control";
        case kVK_Function:
            return "Fn";
        case kVK_Command:
        case kVK_RightCommand:
            return "Meta";
        case kVK_Shift:
        case kVK_RightShift:
            return "Shift";
        case kVK_Return:
        case kVK_ANSI_KeypadEnter:
            return "Enter";
        case kVK_Tab:
            return "Tab";
        case kVK_Space:
            return "Spacebar";
        case kVK_DownArrow:
            return "ArrowDown";
        case kVK_LeftArrow:
            return "ArrowLeft";
        case kVK_RightArrow:
            return "ArrowRight";
        case kVK_UpArrow:
            return "ArrowUp";
        case kVK_End:
            return "End";
        case kVK_Home:
            return "Home";
        case kVK_PageDown:
            return "PageDown";
        case kVK_PageUp:
            return "PageUp";
        case kVK_Delete:
            return "Backspace";
        case kVK_ANSI_KeypadClear:
            return "Clear";
        case kVK_ForwardDelete:
            return "Delete";
        case kVK_Escape:
            return "Escape";
        case kVK_Help:
            return "Help";
        case kVK_F1:
            return "F1";
        case kVK_F2:
            return "F2";
        case kVK_F3:
            return "F3";
        case kVK_F4:
            return "F4";
        case kVK_F5:
            return "F5";
        case kVK_F6:
            return "F6";
        case kVK_F7:
            return "F7";
        case kVK_F8:
            return "F8";
        case kVK_F9:
            return "F9";
        case kVK_F10:
            return "F10";
        case kVK_F11:
            return "F11";
        case kVK_F12:
            return "F12";
        case kVK_F13:
            return "F13";
        case kVK_F14:
            return "F14";
        case kVK_F15:
            return "F15";
        case kVK_F16:
            return "F16";
        case kVK_F17:
            return "F17";
        case kVK_F18:
            return "F18";
        case kVK_F19:
            return "F19";
        case kVK_F20:
            return "F20";
        case kVK_ANSI_KeypadDecimal:
        case kVK_JIS_KeypadComma:
            return ".";
        case kVK_ANSI_KeypadMultiply:
            return "*";
        case kVK_ANSI_KeypadPlus:
            return "+";
        case kVK_ANSI_KeypadDivide:
            return "/";
        case kVK_ANSI_KeypadMinus:
            return "-";
        default:
            if (key_stroke >= 0x52 && key_stroke <= 0x5C) {
                return std::to_string(key_stroke - 0x52);
            } else {
                return ConvertKeyCodeToText(key_stroke);
            }
    }
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(push_to_talk, Init)
