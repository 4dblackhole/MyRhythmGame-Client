# Editor audio fixture

`OppositePhase.ogg` is generated test data, not a user song. It contains one
second of 48 kHz stereo: a half-scale 1 kHz sine wave for 0.7 seconds, then
silence. The right channel is the inverse of the left channel.

It was encoded from signed 16-bit PCM with FFmpeg's libvorbis encoder (default
quality). Tests require no installed FFmpeg or Windows Vorbis codec. The test
compares decoded sample rate/duration, waveform, spectral frequency/level and
silence, and copies the same bytes to Unicode/uppercase/renamed file paths.
