FROM debian:buster-slim
ENV DEBIAN_FRONTEND noninteractive
COPY --chmod=644 r2cloud.gpg /etc/apt/trusted.gpg.d/r2cloud.gpg
RUN echo "deb http://r2cloud.s3.amazonaws.com buster main" >> /etc/apt/sources.list
RUN echo "deb http://r2cloud.s3.amazonaws.com/cpu-generic buster main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y build-essential valgrind cmake libconfig-dev check pkg-config libvolk2-dev libvolk2-bin librtlsdr-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*