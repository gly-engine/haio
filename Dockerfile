FROM debian:forky AS builder

RUN apt update && apt install -y
RUN apt install -y gcc-16 g++-16 cmake make binutils ca-certificates
RUN mkdir -p /app/docs

COPY ./CMakeLists.txt /app/CMakeLists.txt
COPY ./include /app/include
COPY ./library /app/library
COPY ./scripts /app/scripts
COPY ./source /app/source
COPY ./cmake /app/cmake
COPY ./tests /app/tests

WORKDIR /app

ENV CC=gcc-16
ENV CXX=g++-16

RUN cmake -Bbuild -H.
RUN make -C build -j$(nproc --ignore 1)
RUN strip build/bin/haio

FROM scratch

COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt
COPY --from=builder /app/build/bin/haio /bin/haio

ENTRYPOINT ["/bin/haio"]
