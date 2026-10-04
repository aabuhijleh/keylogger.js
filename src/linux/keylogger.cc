#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <napi.h>
#include <poll.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

    // One global TSFN guarded by a mutex. The reader thread takes the mutex for
    // every delivery so Stop() can never release the TSFN while a
    // NonBlockingCall is in flight.
    std::mutex tsfnMutex;
    Napi::ThreadSafeFunction tsfn;

    // Events dropped because the TSFN queue was full. Kept for diagnostics; the
    // reader thread must never block on the JS side, so dropping is by design.
    std::atomic<uint64_t> droppedEvents{0};

    const char *kOpenErrorMessage =
      "keylogger: could not open any keyboard /dev/input/event* node; add the user to group "
      "'input' "
      "or grant an ACL on the device (see README)";

    // Highest /dev/input/eventN index probed during discovery.
    const int kMaxEventNode = 63;

    // Data structure representing our thread-safe function context.
    struct TsfnContext {
        TsfnContext() {
        }
        ~TsfnContext() {
            for (int fd : pipeFds) {
                if (fd >= 0) {
                    close(fd);
                }
            }
        }

        std::thread nativeThread;
        std::promise<bool> openedAnyDevice;
        int pipeFds[2] = {-1, -1};  // self-pipe used by stop to wake the reader
    };

    // A keyboard device kept open by the reader thread.
    struct InputDevice {
        int fd;
        libevdev *dev;
    };

    // Event payload copied by value and handed to the JS thread through the TSFN.
    struct KeyEvent {
        uint16_t code;
        int32_t value;
    };

    void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context);

    // Runs on the JS thread: build the event object and call the dispatch
    // function handed to start().
    void DispatchEvent(Napi::Env env, Napi::Function dispatch, KeyEvent *event) {
        if (event == nullptr) {
            return;
        }
        if (env == nullptr || dispatch == nullptr) {
            delete event;
            return;
        }

        Napi::Object obj = Napi::Object::New(env);
        obj.Set("keyCode", Napi::Number::New(env, event->code));
        obj.Set("extended", Napi::Boolean::New(env, false));
        obj.Set("state", Napi::String::New(env, event->value == 0 ? "up" : "down"));
        obj.Set("repeat", Napi::Boolean::New(env, event->value == 2));
        obj.Set("character", Napi::String::New(env, ""));
        delete event;

        dispatch.Call({obj});
        if (env.IsExceptionPending()) {
            // Never let a throwing dispatch callback kill the reader thread.
            env.GetAndClearPendingException();
        }
    }

    // Runs on the reader thread: hand the event to the JS thread without
    // blocking. On a full queue the event is dropped and counted.
    void DeliverEvent(uint16_t code, int32_t value) {
        auto *event = new KeyEvent{code, value};
        napi_status status = napi_closing;
        {
            std::lock_guard<std::mutex> lock(tsfnMutex);
            if (tsfn) {
                status = tsfn.NonBlockingCall(event, DispatchEvent);
            }
        }
        if (status != napi_ok) {
            delete event;
            if (status == napi_queue_full) {
                droppedEvents.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Drain one readable device. After a SYN_DROPPED the device is resynced per
    // the libevdev docs; events seen during the resync are not real keystrokes
    // and are never delivered.
    void DrainDevice(libevdev *dev) {
        struct input_event ev;
        while (true) {
            int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
            if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                if (ev.type == EV_KEY) {
                    DeliverEvent(ev.code, ev.value);
                }
            } else if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                do {
                    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
                } while (rc == LIBEVDEV_READ_STATUS_SYNC);
                // Resync finished (or failed); real events resume on the next poll.
            } else {
                // -EAGAIN (drained) or an unrecoverable error: move on.
                break;
            }
        }
    }

    void ReaderLoop(int stopFd, std::vector<InputDevice> &devices) {
        std::vector<pollfd> fds;
        fds.push_back({stopFd, POLLIN, 0});
        for (const InputDevice &device : devices) {
            fds.push_back({device.fd, POLLIN, 0});
        }

        while (true) {
            int rc = poll(fds.data(), fds.size(), -1);
            if (rc < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (fds[0].revents & POLLIN) {
                break;  // stop() signaled the self-pipe
            }
            for (size_t i = 0; i < devices.size(); i++) {
                if (fds[i + 1].revents & POLLIN) {
                    DrainDevice(devices[i].dev);
                }
            }
        }
    }

    // Open every keyboard among /dev/input/event0..kMaxEventNode. A keyboard is a
    // device that reports EV_KEY events including KEY_A; that filters out mice,
    // power buttons and consumer-control nodes. Devices are never grabbed, so the
    // desktop keeps seeing the keys.
    void OpenKeyboardDevices(std::vector<InputDevice> &devices) {
        for (int i = 0; i <= kMaxEventNode; i++) {
            std::string path = "/dev/input/event" + std::to_string(i);
            int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                continue;
            }
            libevdev *dev = nullptr;
            if (libevdev_new_from_fd(fd, &dev) != 0) {
                close(fd);
                continue;
            }
            if (libevdev_has_event_type(dev, EV_KEY) &&
                libevdev_has_event_code(dev, EV_KEY, KEY_A)) {
                devices.push_back({fd, dev});
            } else {
                libevdev_free(dev);
                close(fd);
            }
        }
    }

    void ReaderThreadMain(TsfnContext *context) {
        std::vector<InputDevice> devices;
        OpenKeyboardDevices(devices);
        context->openedAnyDevice.set_value(!devices.empty());
        if (devices.empty()) {
            return;
        }
        ReaderLoop(context->pipeFds[0], devices);
        for (InputDevice &device : devices) {
            libevdev_free(device.dev);
            close(device.fd);
        }
    }

    // Release the TSFN if one is active. Safe to call when not listening. The
    // finalizer owns shutdown: it signals the self-pipe, joins the reader thread
    // (which frees the devices) and deletes the context. The handle is swapped
    // out under the mutex but released outside of it: once the global is empty
    // the reader thread stops delivering, and Release() must not run while the
    // mutex is held because the finalizer joins the reader thread, which may be
    // waiting on the mutex itself.
    void ReleaseTsfn() {
        Napi::ThreadSafeFunction toRelease;
        {
            std::lock_guard<std::mutex> lock(tsfnMutex);
            toRelease = tsfn;
            tsfn = nullptr;
        }
        if (toRelease) {
            toRelease.Release();
        }
    }

    // Called from JS with a dispatch function as an argument. The dispatch
    // function is called on the JS thread with one event object per key press or
    // release.
    void Start(const Napi::CallbackInfo &info) {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsFunction()) {
            Napi::TypeError::New(env, "keylogger.start(dispatch): dispatch must be a function")
              .ThrowAsJavaScriptException();
            return;
        }

        // Stop a previous session if start() is called while already listening.
        ReleaseTsfn();

        auto *context = new TsfnContext();
        if (pipe(context->pipeFds) != 0) {
            delete context;
            Napi::Error::New(env, "keylogger: could not create the stop signal pipe")
              .ThrowAsJavaScriptException();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(tsfnMutex);
            tsfn = Napi::ThreadSafeFunction::New(
              env,
              info[0].As<Napi::Function>(),  // JavaScript function called asynchronously
              "Keyboard Events",             // Name
              1024,                          // Bounded queue; overflow drops events
              1,                             // Only one thread will use this initially
              context,                       // Context that can be accessed by Finalizer
              FinalizerCallback,             // Finalizer used to clean threads up
              (void *)nullptr                // Finalizer data
            );
        }
        if (!tsfn) {
            delete context;
            Napi::Error::New(env, "keylogger: could not create the thread-safe function")
              .ThrowAsJavaScriptException();
            return;
        }

        // The reader thread opens the devices so Start() can report synchronously
        // when no keyboard node is readable (usually a permission problem).
        std::future<bool> openedAny = context->openedAnyDevice.get_future();
        context->nativeThread = std::thread(ReaderThreadMain, context);

        if (!openedAny.get()) {
            if (context->nativeThread.joinable()) {
                context->nativeThread.join();
            }
            ReleaseTsfn();  // runs the finalizer, which deletes the context
            Napi::Error::New(env, kOpenErrorMessage).ThrowAsJavaScriptException();
            return;
        }
    }

    // Called from JS to release the TSFN and stop listening to keyboard events.
    // Idempotent: safe when never started and safe to call twice.
    void Stop(const Napi::CallbackInfo &info) {
        ReleaseTsfn();
    }

    void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context) {
        // Wake the reader thread so it leaves poll() and frees the devices.
        if (context->pipeFds[1] >= 0) {
            uint8_t byte = 1;
            ssize_t written = write(context->pipeFds[1], &byte, sizeof(byte));
            (void)written;
        }
        if (context->nativeThread.joinable()) {
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

}  // namespace

NODE_API_MODULE(keylogger, Init)
