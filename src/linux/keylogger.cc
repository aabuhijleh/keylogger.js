#include <linux/input.h>
#include <napi.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <thread>

#include "find_event_file.hh"

#define NUM_EVENTS 128
#define NUM_KEYCODES 71

const char *keycodes[] = {
  "RESERVED",   "ESC",   "1",         "2",          "3",     "4",        "5",          "6",
  "7",          "8",     "9",         "0",          "MINUS", "EQUAL",    "BACKSPACE",  "TAB",
  "Q",          "W",     "E",         "R",          "T",     "Y",        "U",          "I",
  "O",          "P",     "LEFTBRACE", "RIGHTBRACE", "ENTER", "LEFTCTRL", "A",          "S",
  "D",          "F",     "G",         "H",          "J",     "K",        "L",          "SEMICOLON",
  "APOSTROPHE", "GRAVE", "LEFTSHIFT", "BACKSLASH",  "Z",     "X",        "C",          "V",
  "B",          "N",     "M",         "COMMA",      "DOT",   "SLASH",    "RIGHTSHIFT", "KPASTERISK",
  "LEFTALT",    "SPACE", "CAPSLOCK",  "F1",         "F2",    "F3",       "F4",         "F5",
  "F6",         "F7",    "F8",        "F9",         "F10",   "NUMLOCK",  "SCROLLLOCK"};

Napi::ThreadSafeFunction tsfn;

struct TsfnContext {
    TsfnContext(Napi::Env env){};
    int shouldNativeThreadKeepRunning = 1;
    std::thread nativeThread;
};

void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context) {
    std::cerr << "Failed to join nativeThread!" << std::endl;
    context->shouldNativeThreadKeepRunning = 0;
    if (context->nativeThread.joinable()) {
        context->nativeThread.join();
    } else {
        std::cerr << "Failed to join nativeThread!" << std::endl;
    }

    delete context;
}

void ReleaseTSFN() {
    if (tsfn) {
        napi_status status = tsfn.Release();
        if (status != napi_ok) {
            std::cerr << "Failed to release the TSFN!" << std::endl;
        }
        tsfn = NULL;
    }
}

void Start(const Napi::CallbackInfo &info) {
    std::cout << "Start..." << std::endl;

    Napi::Env env = info.Env();

    ReleaseTSFN();

    auto contextData = new TsfnContext(env);

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

    contextData->nativeThread = std::thread([contextData] {
        std::cout << "nativeThread1" << std::endl;

        char *KEYBOARD_DEVICE = get_keyboard_event_file();
        if (!KEYBOARD_DEVICE) {
            std::cerr << "Could not find keyboard file" << std::endl;
            return;
        }

        int keyboard;
        if ((keyboard = open(KEYBOARD_DEVICE, O_RDONLY)) < 0) {
            std::cerr << "Error accessing keyboard from " << KEYBOARD_DEVICE
                      << ". May require you to be superuser\n"
                      << std::endl;
            return;
        }

        int eventSize = sizeof(struct input_event);
        int bytesRead = 0;
        struct input_event events[NUM_EVENTS];
        int i;

        std::cout << "nativeThread2" << keyboard << std::endl;

        while (contextData->shouldNativeThreadKeepRunning) {
            std::cout << "loop1 " << std::endl;
            bytesRead = read(keyboard, events, eventSize * NUM_EVENTS);
            std::cout << "loop2 " << std::endl;

            for (i = 0; i < (bytesRead / eventSize); ++i) {
                if (events[i].type == EV_KEY) {
                    if (events[i].value == 1) {
                        if (events[i].code > 0 && events[i].code < NUM_KEYCODES) {
                            std::cout << "KEY: " << keycodes[events[i].code] << " -- "
                                      << events[i].code << std::endl;
                        } else {
                            std::cout << "KEY: "
                                      << "UNRECOGNIZED" << std::endl;
                        }
                    }
                }
            }
        }
        if (bytesRead > 0)
            std::cout << "KEY: "
                      << "EOF" << std::endl;

        close(keyboard);
        free(KEYBOARD_DEVICE);
    });
}

void Stop(const Napi::CallbackInfo &info) {
    std::cout << "Stop..." << std::endl;
    ReleaseTSFN();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(keylogger, Init)