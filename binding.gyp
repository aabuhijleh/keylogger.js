{
    "targets": [
        {
            "target_name": "keylogger",
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "conditions":[
                ["OS=='mac'", {
                    "sources": ["src/macOS/keylogger.mm", "src/macOS/string_conversion.cc"],
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS": ["-std=c++11", "-stdlib=libc++", "-mmacosx-version-min=10.10"],
                        "OTHER_LDFLAGS": ["-framework Cocoa"]
                    },
                }],
                ["OS=='win'", {
                    "sources": ["src/windows/keylogger.cc"]
                }]
            ],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")"
            ],
            'defines': ['NAPI_DISABLE_CPP_EXCEPTIONS'],
        }
    ]
}
