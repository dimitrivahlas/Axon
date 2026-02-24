FROM alpine:3.19

RUN apk add --no-cache \
    gcc \
    musl-dev \
    make \
    curl-dev \
    valgrind \
    gdb

WORKDIR /axon

COPY . .

RUN make build

CMD ["./axon"]
