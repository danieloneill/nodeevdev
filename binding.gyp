{
  "targets": [
    {
      "target_name": "evdev",
      "sources": [ "src/evdev.cpp" ],
      'cflags': [
        '<!@(pkg-config --cflags libevdev)'
      ],
      'ldflags': [
        '<!@(pkg-config --libs-only-L --libs-only-other libevdev)'
      ],
      'libraries': [
        '<!@(pkg-config --libs-only-l libevdev)'
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ]
    }
  ]
}
