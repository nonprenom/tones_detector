
**<span class="underline">A real time frequency finder by reading the
PCM input.</span>**

The tones (ring, busy, etc...) is defined by a frequency and a pattern:

Ring: 425 Hz for 3s then silent for 1 second.

![](media/image1.png)Busy: 400 Hz for 500ms then silent for 500ms

while the DTMF is normalized in the phone protocol, the other tones are
specific to a country: the frequency change and the pattern

You can query all the patterns and frequencies for all the country on
this online database:

<https://www.3amsystems.com/World_Tone_Database?q=Israel,Busy_tone>

For example, for Israel, the busy tone is: 400Hs – 0.5s on, 0.5s Off

**<span class="underline">Implementation</span>**

For this I used the Goertzel algorithm which is usually used for DTMF
detection. It’s not a FFT (Fourier) because it gives only a magnitude
for **one** frequency. It’s much easier to implement (15 lines in C) and
specially adapted for embedded compare to an FFT.

Now, that I’m able to detect a frequency over the time, I implemented a
pattern detector.

First, to avoid false detections, I’m not searching for \[0.5 On, 0.5
Off\] but for 4 of them like this:

0.5 off, 0.5 On, 0.5 off, 0.5 On, 0.5 off

Because the pattern is not 100% perfect on real samples (sometimes it’s
0.45 off, 0.52 on, 0.42 off, ...) I’m working on a statistic: how much %
the real signal is close to the one I’m looking for.

I then build a definition file for RING and BUSY for all the country we
are working with.

With a required magnitude of 95% and after I downloaded all the samples
from the DB above, I’m able to detect all the busy tone.

Note: to clean the signal I'm using the average of the last 20 samples.

Note: I integrated this on a real time embedded platform. For this I
start the detector inside a thread which take the PCM sample, not from
the file like in this code, but from the PCM device.

Whenever a tone is found it call a callback to the main thread.

Below it’s a representation of the results in excel from a record on a
real call. In orange it’s the Goertzel magnitude computed, in gray, it’s
after I cleaned it and before pattern detection. The session is RING \*
6 \> DECLINE \> BUSY \*25

![](https://i.imgur.com/iNE40ad.png)

Below are the results

> Israel\_call\_from\_the\_CP2.pcm
> 
> p=13 res=1 \[Israel\] \[Ring tone\] pat\_size=1100 dur=11000.000
> start=0.000 end=11.000
> 
> p=13 res=1 \[Israel\] \[Ring tone\] pat\_size=1100 dur=11000.000
> start=11.370 end=22.370
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=23.320 end=25.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=26.320 end=28.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=29.320 end=31.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=32.320 end=34.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=35.320 end=37.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=38.320 end=40.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=41.320 end=43.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=44.320 end=46.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=47.320 end=49.820
> 
> p=14 res=1 \[Israel\] \[Busy tone\] pat\_size=250 dur=2500.000
> start=50.320 end=52.820

**<span class="underline">How to build and run (on Linux here)</span>**

> gcc detect.c -o detect -lm && rm -f detect.txt && ./detect \>
> detect.csv

It will compute all the PCM samples from the directory "pcm8kMono16ble".

It outputs the results in the terminal and the goertzel + the signal
after cleaning in a CSV file. You can import this in Excel.

Just keep one sample at the beginning.

**<span class="underline">Others</span>**

Ffmpeg tools on windows

Convert a wav to a wav 8000 Hz / Mono / 16 bits LE

> "c:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe" -i sample.wav -ar 8000
> -ac 1 -acodec pcm\_u8 sample8kMono.wav

Convert all the wav of a directory to PCM 8k Mono 16 bits LE

> for %i in (../wav/\*) do "c:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe"
> -y > -i ../wav/%i -ar 8000 -ac 1 -f s16le -acodec pcm\_s16le %\~ni.pcm

**<span class="underline">Useful links</span>**

<http://en.wikipedia.org/wiki/Goertzel_algorithm>

<https://www.embedded.com/the-goertzel-algorithm/>

<https://github.com/Harvie/Programs/blob/master/c/goertzel/goertzel.c>

<https://stackoverflow.com/questions/67556935/busy-tone-detection-in-audio-pcm-signal>
