#include <Carbon/Carbon.h>
#include <napi.h>

#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace {

    // One global TSFN, guarded by tsfnMutex for lifecycle changes on the JS
    // thread. The event-tap thread reads it lock-free; it is joined inside the
    // finalizer before the global is reset, so it never observes the reset.
    Napi::ThreadSafeFunction tsfn;
    std::mutex tsfnMutex;

    // Events dropped because the TSFN queue (bounded at 1024) was full.
    std::atomic<uint64_t> droppedEvents{0};

    // Everything the event-tap thread needs, owned by the TSFN finalizer.
    struct TsfnContext {
        std::thread nativeThread;
        // Published by the native thread before it resolves tapCreated, so the
        // finalizer can always stop the run loop once Start() has returned.
        CFRunLoopRef runLoop = nullptr;
        // Owned by the native thread; the tap callback re-enables it through this.
        CFMachPortRef eventTap = nullptr;
        // Modifier keyCodes currently held down, tapped-thread only, used to
        // derive down/up from kCGEventFlagsChanged events.
        std::set<CGKeyCode> pressedModifiers;
        // Resolved by the native thread once CGEventTapCreate has succeeded or
        // definitively failed, so Start() can throw synchronously.
        std::promise<bool> tapCreated;
    };

    // Event payload copied by value on the tap thread and handed to the JS
    // thread through the TSFN queue.
    struct KeyEventData {
        CGKeyCode keyCode;
        CGEventFlags flags;
        bool keyUp;
        bool repeat;
    };

    // Cached keyboard layout for UCKeyTranslate. source is retained and keeps the
    // layout data alive; the CFNotificationCenter observer below releases and
    // clears the cache when the input source changes so the next event re-copies.
    struct LayoutCache {
        std::mutex mutex;
        TISInputSourceRef source = nullptr;
        const UCKeyboardLayout *layout = nullptr;
        UInt32 keyboardType = 0;
    };
    LayoutCache layoutCache;

    void ClearLayoutCacheLocked() {
        if (layoutCache.source != nullptr) {
            CFRelease(layoutCache.source);
            layoutCache.source = nullptr;
            layoutCache.layout = nullptr;
            layoutCache.keyboardType = 0;
        }
    }

    void ClearLayoutCache() {
        std::lock_guard<std::mutex> lock(layoutCache.mutex);
        ClearLayoutCacheLocked();
    }

    void InputSourceChangedCallback(CFNotificationCenterRef center, void *observer,
                                    CFStringRef name, const void *object,
                                    CFDictionaryRef userInfo) {
        ClearLayoutCache();
    }

    // Fills the layout cache; caller must hold layoutCache.mutex. The layout
    // pointer borrowed from the source's kTISPropertyUnicodeKeyLayoutData stays
    // valid as long as the source is retained.
    bool EnsureLayoutCacheLocked() {
        if (layoutCache.source != nullptr) {
            return true;
        }

        TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
        CFDataRef layoutData = nullptr;
        if (source != nullptr) {
            layoutData = static_cast<CFDataRef>(
              TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
        }
        if (layoutData == nullptr) {
            // TISGetInputSourceProperty returns null with the Japanese keyboard
            // layout. Using TISCopyCurrentKeyboardLayoutInputSource to fix NULL
            // return.
            if (source != nullptr) {
                CFRelease(source);
            }
            source = TISCopyCurrentKeyboardLayoutInputSource();
            if (source != nullptr) {
                layoutData = static_cast<CFDataRef>(
                  TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
            }
        }
        if (source == nullptr || layoutData == nullptr) {
            if (source != nullptr) {
                CFRelease(source);
            }
            return false;
        }

        layoutCache.source = source;
        layoutCache.layout =
          reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(layoutData));

        SInt32 keyboardType = 0;
        CFNumberRef typeNumber =
          static_cast<CFNumberRef>(TISGetInputSourceProperty(source, kTISPropertyKeyboardType));
        if (typeNumber != nullptr) {
            CFNumberGetValue(typeNumber, kCFNumberSInt32Type, &keyboardType);
        }
        layoutCache.keyboardType = static_cast<UInt32>(keyboardType);
        return true;
    }

    // Minimal UTF-16 -> UTF-8 conversion, written for this file.
    std::string Utf16ToUtf8(const UniChar *input, size_t length) {
        std::string output;
        for (size_t i = 0; i < length; i++) {
            uint32_t codePoint = input[i];
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < length) {
                uint32_t low = input[i + 1];
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    i++;
                }
            }
            if (codePoint < 0x80) {
                output += static_cast<char>(codePoint);
            } else if (codePoint < 0x800) {
                output += static_cast<char>(0xC0 | (codePoint >> 6));
                output += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else if (codePoint < 0x10000) {
                output += static_cast<char>(0xE0 | (codePoint >> 12));
                output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                output += static_cast<char>(0x80 | (codePoint & 0x3F));
            } else {
                output += static_cast<char>(0xF0 | (codePoint >> 18));
                output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                output += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }
        return output;
    }

    bool IsControlCharacter(UniChar character) {
        return character < 0x20 || (character >= 0x7F && character <= 0x9F);
    }

    // Layout-translated character for a key press, or "" for keys with no
    // printable translation. Runs on the JS thread inside the TSFN callback.
    std::string TranslateKeyCode(CGKeyCode keyCode, CGEventFlags flags) {
        EventModifiers carbonModifiers = 0;
        if (flags & kCGEventFlagMaskShift) {
            carbonModifiers |= shiftKey;
        }
        if (flags & kCGEventFlagMaskControl) {
            carbonModifiers |= controlKey;
        }
        if (flags & kCGEventFlagMaskAlternate) {
            carbonModifiers |= optionKey;
        }
        if (flags & kCGEventFlagMaskCommand) {
            carbonModifiers |= cmdKey;
        }
        // UCKeyTranslate accepts the Carbon modifier bits shifted right by 8.
        UInt32 modifierKeyState = (carbonModifiers >> 8) & 0xFF;

        std::lock_guard<std::mutex> lock(layoutCache.mutex);
        if (!EnsureLayoutCacheLocked()) {
            return std::string();
        }

        UInt32 deadKeyState = 0;
        UniChar character = 0;
        UniCharCount charCount = 0;
        OSStatus status =
          UCKeyTranslate(layoutCache.layout, static_cast<UInt16>(keyCode), kUCKeyActionDown,
                         modifierKeyState, layoutCache.keyboardType, kUCKeyTranslateNoDeadKeysBit,
                         &deadKeyState, 1, &charCount, &character);
        if (status != noErr || charCount != 1 || IsControlCharacter(character)) {
            return std::string();
        }
        return Utf16ToUtf8(&character, 1);
    }

    // TSFN callback. Runs on the JS thread; owns `data`.
    void DispatchKeyEvent(Napi::Env env, Napi::Function dispatch, KeyEventData *data) {
        std::unique_ptr<KeyEventData> event(data);

        Napi::Object raw = Napi::Object::New(env);
        raw.Set("keyCode", Napi::Number::New(env, event->keyCode));
        raw.Set("extended", Napi::Boolean::New(env, false));  // always false on macOS
        raw.Set("state", Napi::String::New(env, event->keyUp ? "up" : "down"));
        raw.Set("repeat", Napi::Boolean::New(env, event->repeat));
        raw.Set("character",
                Napi::String::New(env, TranslateKeyCode(event->keyCode, event->flags)));

        dispatch.Call({raw});
        if (env.IsExceptionPending()) {
            // A throwing dispatch must not escape the TSFN callback; there is no
            // JS frame above it to catch the exception.
            env.GetAndClearPendingException();
            return;
        }
    }

    // Event-tap callback. Runs on the native thread's run loop and must return
    // immediately: copy the event data by value, hand it to the TSFN, return.
    CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event,
                               void *refcon) {
        TsfnContext *context = static_cast<TsfnContext *>(refcon);

        if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
            if (context->eventTap != nullptr) {
                CGEventTapEnable(context->eventTap, true);
            }
            return event;
        }
        if (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged) {
            return event;
        }

        CGKeyCode keyCode =
          static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));

        bool keyUp = false;
        if (type == kCGEventKeyUp) {
            keyUp = true;
        } else if (type == kCGEventFlagsChanged) {
            if (context->pressedModifiers.count(keyCode) > 0) {
                context->pressedModifiers.erase(keyCode);
                keyUp = true;
            } else {
                context->pressedModifiers.insert(keyCode);
            }
        }
        bool repeat = type == kCGEventKeyDown &&
                      CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;

        KeyEventData *data = new KeyEventData{keyCode, CGEventGetFlags(event), keyUp, repeat};
        napi_status status = tsfn.NonBlockingCall(data, DispatchKeyEvent);
        if (status == napi_queue_full) {
            // NAPI's internal queue cannot evict the oldest entry, so drop the
            // incoming event and count it.
            delete data;
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
        } else if (status != napi_ok) {
            delete data;
        }
        return event;
    }

    void NativeThreadMain(TsfnContext *context) {
        context->runLoop = CFRunLoopGetCurrent();

        CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                                CGEventMaskBit(kCGEventFlagsChanged);
        // A passive listen-only tap is tried first because it can never swallow
        // keystrokes. The fallback to an active tap exists because libuiohook
        // cites its bug #22 against listen-only taps; it stays until a run on a
        // Mac proves listen-only works.
        CFMachPortRef eventTap =
          CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
                           eventMask, CGEventCallback, context);
        if (eventTap == nullptr) {
            eventTap =
              CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                               eventMask, CGEventCallback, context);
        }
        if (eventTap == nullptr) {
            context->tapCreated.set_value(false);
            return;
        }
        context->eventTap = eventTap;

        CFRunLoopSourceRef runLoopSource =
          CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
        CFRunLoopAddSource(context->runLoop, runLoopSource, kCFRunLoopCommonModes);
        CGEventTapEnable(eventTap, true);

        context->tapCreated.set_value(true);
        CFRunLoopRun();

        CGEventTapEnable(eventTap, false);
        CFRunLoopRemoveSource(context->runLoop, runLoopSource, kCFRunLoopCommonModes);
        CFMachPortInvalidate(eventTap);
        CFRelease(runLoopSource);
        CFRelease(eventTap);
        context->eventTap = nullptr;
    }

    // The finalizer owns thread shutdown: stop the run loop, join, delete the
    // context. Runs when the TSFN's thread count reaches zero.
    void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context) {
        if (context->runLoop != nullptr) {
            CFRunLoopStop(context->runLoop);
        }
        if (context->nativeThread.joinable()) {
            context->nativeThread.join();
        } else {
            std::cerr << "keylogger: failed to join nativeThread!" << std::endl;
        }
        ClearLayoutCache();
        delete context;
    }

    // Release the TSFN. Safe to call when never started and when already stopped.
    void ReleaseTSFN() {
        std::lock_guard<std::mutex> lock(tsfnMutex);
        if (tsfn) {
            napi_status status = tsfn.Release();
            if (status != napi_ok) {
                std::cerr << "keylogger: failed to release the TSFN!" << std::endl;
            }
            tsfn = nullptr;
        }
    }

    // Surface the overflow count once per stop() instead of letting it
    // accumulate silently, then reset it for the next listener.
    void ReportDroppedEvents() {
        uint64_t dropped = droppedEvents.exchange(0, std::memory_order_relaxed);
        if (dropped > 0) {
            std::cerr << "keylogger: dropped " << dropped
                      << " keyboard events because the event queue was full" << std::endl;
        }
    }

}  // namespace

// Trigger the JS callback when a key is pressed or released.
// dispatch: ({ keyCode, extended, state, repeat, character }) => void
void Start(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "keylogger.start(dispatch): dispatch must be a function")
          .ThrowAsJavaScriptException();
        return;
    }

    // Calling start twice is prevented by the JS wrapper; release any
    // previous TSFN first anyway.
    ReleaseTSFN();

    auto *contextData = new TsfnContext();
    std::future<bool> tapCreated = contextData->tapCreated.get_future();

    // Create a ThreadSafeFunction
    tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
      "Keyboard Events",             // Name
      1024,                          // Bounded queue; overflow is dropped and counted
      1,                             // Only one thread will use this initially
      contextData,                   // Context that can be accessed by Finalizer
      FinalizerCallback,             // Finalizer used to clean threads up
      (void *)nullptr                // Finalizer data
    );

    contextData->nativeThread = std::thread(NativeThreadMain, contextData);

    if (!tapCreated.get()) {
        // The finalizer joins the (already exited) thread and deletes the
        // context before the throw.
        ReleaseTSFN();
        Napi::Error::New(env,
                         "keylogger: failed to create a CGEventTap; grant the app assistive access "
                         "(Accessibility / Input Monitoring) in System Settings")
          .ThrowAsJavaScriptException();
        return;
    }
}

void Stop(const Napi::CallbackInfo &info) {
    ReleaseTSFN();
    ReportDroppedEvents();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), nullptr,
                                    InputSourceChangedCallback,
                                    kTISNotifySelectedKeyboardInputSourceChanged, nullptr,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);

    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(keylogger, Init)
