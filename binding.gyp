{
    "targets": [
        {
            "target_name": "keylogger",
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "cflags_cc": ["-std=c++17"],
            "conditions":[
                ["OS=='mac'", {
                    "sources": ["src/macOS/keylogger.mm"],
                    "xcode_settings": {
                        "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
                        "CLANG_CXX_LIBRARY": "libc++",
                        "MACOSX_DEPLOYMENT_TARGET": "11.0",
                        "OTHER_LDFLAGS": ["-framework Cocoa", "-framework Carbon"]
                    }
                }],
                ["OS=='win'", {
                    "sources": ["src/windows/keylogger.cc"],
                    "msvs_settings": {
                        "VCCLCompilerTool": {
                            "AdditionalOptions": ["/std:c++17"]
                        }
                    }
                }],
                ["OS=='linux'", {
                    "sources": ["src/linux/keylogger.cc"],
                    "libraries": ["-levdev"]
                }]
            ],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")"
            ],
            "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS", "NAPI_VERSION=8"]
        }
    ]
}
