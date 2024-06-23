FROM debian:stretch-slim
ENV DEBIAN_FRONTEND noninteractive
COPY --chmod=644 r2cloud.gpg /etc/apt/trusted.gpg.d/r2cloud.gpg
RUN echo 'deb http://archive.debian.org/debian/ stretch main non-free contrib' > /etc/apt/sources.list
RUN echo 'deb http://archive.debian.org/debian-security/ stretch/updates main non-free contrib' >> /etc/apt/sources.list
RUN echo "deb http://r2cloud.s3.amazonaws.com stretch main" >> /etc/apt/sources.list
RUN echo "deb http://r2cloud.s3.amazonaws.com/cpu-generic stretch main" >> /etc/apt/sources.list
RUN apt-get update && apt-get install --no-install-recommends -y build-essential valgrind cmake libconfig-dev check pkg-config libvolk2-dev libvolk2-bin librtlsdr-dev zlib1g-dev && rm -rf /var/lib/apt/lists/*