# stb_vorbis

Upstream: https://github.com/nothings/stb
Revision: `2c980bb59875b0d32144a71867fbdebb2f77cd20`
File: `stb_vorbis.c`, v1.22 (unmodified)
SHA-256: `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636`

The editor uses its sequential Ogg Vorbis float decoder. It does not own game
playback, a device, the rhythm timer, or FMOD objects. The license is retained
here and embedded as `Assets/Licenses/stb-vorbis.txt`.

`ThirdParty/StbVorbis.cpp` compiles the upstream file once. The owned Vorbis
adapter includes only its declaration section (`STB_VORBIS_HEADER_ONLY`).
The vendor directory is an MSVC external include. Its implementation is outside
Client-owned static analysis; owned decoder/FFT code remains at /W4 and is analyzed.

One compiler flow diagnostic, C4701, is targeted in that wrapper: upstream's
`seek_to_sample_coarse` increments `probe` only after `get_seek_page_info` fills
`mid`; its `probe == 1` branch therefore cannot read `mid` before initialization.
The wrapper documents this invariant instead of changing the vendored source or
disabling Client-owned diagnostics. The editor itself does not use the seeking API.

The upstream decoder supports Vorbis, not other Ogg codecs such as Opus. Its
documented limits include chained streams and pre-2004 floor-0 Vorbis streams.
