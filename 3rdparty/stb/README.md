# stb

[stb_image.h](stb_image.h) (v2.30) decodes the PNG and JPEG images of the 3D
cockpits, and [stb_image_write.h](stb_image_write.h) (v1.16) writes the PNG
screenshots. Both are by Sean Barrett and others, from
<https://github.com/nothings/stb> at commit
`2c980bb59875b0d32144a71867fbdebb2f77cd20`, unchanged. They're in the public
domain, or under the MIT license, whichever you prefer (see the end of each
file).

SHA-256:

```
594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3  stb_image.h
cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05  stb_image_write.h
```

[src/gfx/image.cpp](../../src/gfx/image.cpp) compiles their implementations.
