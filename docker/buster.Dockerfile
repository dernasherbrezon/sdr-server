FROM debian:buster-slim
ENV DEBIAN_FRONTEND noninteractive
COPY --chmod=644 r2cloud.gpg /etc/apt/trusted.gpg.d/r2cloud.gpg
RUN echo "deb http://r2cloud.s3.amazonaws.com buster main" >> /etc/apt/sources.list
RUN echo "deb http://r2cloud.s3.amazonaws.com/cpu-generic buster main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y build-essential file valgrind cmake libconfig-dev pkg-config libairspy-dev libhackrf-dev librtlsdr-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*