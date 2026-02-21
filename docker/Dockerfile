FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libcurl4-openssl-dev \
    valgrind \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /axon

COPY . .

CMD ["make", "run"]
