# About [![Build Status](https://travis-ci.com/dernasherbrezon/sdr-server.svg?branch=main)](https://travis-ci.com/dernasherbrezon/sdr-server) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=dernasherbrezon_sdr-server&metric=alert_status)](https://sonarcloud.io/dashboard?id=dernasherbrezon_sdr-server)

![design](/docs/dsp.jpg?raw=true)

## Key features

 * Share available RF bandwidth between several independent clients:
   * Total bandwidth can be 2016000 samples/sec at 436,600,000 hz
   * One client might request 48000 samples/sec at 436,700,000 hz
   * Another client might request 96000 samples/sec at 435,000,000 hz
 * Several clients can access the same band simultaneously
 * Output saved onto disk
 * Output can be gzipped (by default = true)
 * Output will be decimated to the requested bandwidth
 * Clients can request overlapping RF spectrum
 * Rtl-sdr starts only after first client connects (i.e. save solar power &etc). Stops only when last client disconnects
 * MacOS and Linux (Debian Raspberrypi)
 
## Design

![design](/docs/threads.png?raw=true)

 * Each client has its own dsp thread
 * Each dsp thread executes [Frequency Xlating FIR Filter](http://blog.sdr.hu/grblocks/xlating-fir.html)
 * [Libvolk](https://www.libvolk.org) is used for SIMD optimizations
 * Only RTL-SDRs are supported
 
## API

 * Defined in the [api.h](https://github.com/dernasherbrezon/sdr-server/blob/main/src/api.h)
 * Clients can connect and send request to initiate listening:
   * center_freq - this is required center frequency. For example, 436,700,000 hz
   * sampling_rate - required sampling rate. For example, 48000
   * band\_freq - first connected client can select the center of the band. All other clients should request center\_freq within the currently selected band
 * To stop listening, clients can send SHUTDOWN request or disconnect
 
## Queue

![design](/docs/queue.png?raw=true)

The data between rtl-sdr worker and the dsp workers is passed via queue. This is bounded queue with pre-allocated memory blocks. It has the following features:

 * Thread-safe
 * If no free blocks (consumer is slow), then the last block will be overriden by the next one
 * there is a special detached block. It is used to minimize synchronization section. All potentially long operations on it are happening outside of synchronization section.
 * Consumer will block and wait until new data produced
 
## Performance

Is good. Some numbers in ```test/perf_xlating.c```
 
## Dependencies

sdr-server depends on several libraries:

 * [libvolk](https://www.libvolk.org). It is recommended to use the latest version. After libvolk installed or built, it needs to detect optimal kernels. Run the command ```volk_profile``` to generate and save profile.
 * [librtlsdr](https://github.com/steve-m/librtlsdr)
 * [libconfig](https://hyperrealm.github.io/libconfig/libconfig_manual.html)
 * libz. Should be installed in every operational system
 * libm. Same
 * [libcheck](https://libcheck.github.io/check/) for tests (Optional)
 
All dependencies can be easily installed using the command:

```
sudo apt-get install libvolk2-dev librtlsdr-dev libconfig-dev check
```

## Build

To build the project execute the following commands:

```
mkdir build
cd build
cmake ..
make
```
