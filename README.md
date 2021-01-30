# About [![Build Status](https://travis-ci.com/dernasherbrezon/sdr-server.svg?branch=main)](https://travis-ci.com/dernasherbrezon/sdr-server) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=dernasherbrezon_sdr-server&metric=alert_status)](https://sonarcloud.io/dashboard?id=dernasherbrezon_sdr-server)

![design](/docs/dsp.jpg?raw=true)

## Key features

 * share available RF bandwidth between several independent clients:
   * total bandwidth can be 2016000 samples/sec at 436,600,000 hz
   * one client might request 48000 samples/sec at 436,700,000 hz
   * another client might request 96000 samples/sec at 435,000,000 hz
 * several clients can access the same band simultaneously
 * output saved onto disk
 * output can be gzipped (by default = true)
 * output will be decimated to the requested bandwidth
 * clients can request overlapping RF spectrum
 
## Design

![design](/docs/threads.png?raw=true)

 * Each client has its own dsp thread
 * Each dsp thread executes [Frequency Xlating FIR Filter](http://blog.sdr.hu/grblocks/xlating-fir.html)
 * [libvolk](https://www.libvolk.org) is used for SIMD optimizations
 * only RTL-SDRs are supported
 
## Queue

![design](/docs/queue.png?raw=true)

The data between rtl-sdr worker and the dsp workers is passed via queue. This is bounded queue with pre-allocated memory blocks. It has the following features:

 * thread-safe
 * if no free blocks (consumer is slow), then the last block will be overriden by the next one
 * there is a special detached block. It is used to minimize synchronization section. All potentially long operations on it are happening outside of synchronization section.
 * consumer will block and wait until new data produced
