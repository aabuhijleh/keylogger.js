void Start(const Napi::CallbackInfo &info) {
    // no-op
}

void Stop(const Napi::CallbackInfo &info) {
    // no-op
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
    exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
    return exports;
}

NODE_API_MODULE(keylogger, Init)