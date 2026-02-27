FROM alpine:3.19

RUN apk add --no-cache \
    gcc \
    g++ \
    musl-dev \
    make \
    curl-dev \
    sqlite-dev \
    sqlite \
    git \
    valgrind \
    gdb

WORKDIR /axon

COPY . .

RUN make clean && make build

CMD ["./axon"]
