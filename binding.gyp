{
    "targets": [
        {
            "target_name": "push_to_talk",
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "conditions":[
                ["OS=='mac'", {
                    "sources": ["src/macOS/push-to-talk.mm"],
                    "xcode_settings": {
                        "OTHER_CPLUSPLUSFLAGS": ["-std=c++11", "-stdlib=libc++", "-mmacosx-version-min=10.10"],
                        "OTHER_LDFLAGS": ["-framework CoreFoundation -framework IOKit -framework AppKit"]
                    },
                }],
                ["OS=='win'", {
                    "sources": ["src/windows/push-to-talk.cc"]
                }]
            ],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")"
            ],
            'defines': ['NAPI_DISABLE_CPP_EXCEPTIONS'],
        }
    ]
}
