FROM ubuntu:24.04 AS build

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake qt6-base-dev python3 python3-venv ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY relay/conanfile.py relay/conandata.yml ./
RUN python3 -m venv /opt/conan \
    && /opt/conan/bin/pip install conan==2.34.0 \
    && /opt/conan/bin/conan profile detect \
    && /opt/conan/bin/conan install . --output-folder=/deps --build=missing \
       -s build_type=Release -s compiler.cppstd=20

COPY common /common
COPY relay/ .
ARG TARGETARCH
RUN python3 download_dependencies.py "$TARGETARCH" .deps
RUN cmake -S . -B /build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/deps/generators/conan_toolchain.cmake \
    && cmake --build /build --parallel

FROM ubuntu:24.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libqt6core6t64 libqt6network6t64 ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home relay \
    && mkdir -p /var/lib/relay && chown relay:relay /var/lib/relay

COPY --from=build /build/obs-relay /build/go-irl /build/mediamtx /usr/local/bin/
USER relay
ENV RELAY_CONFIG_DIR=/var/lib/relay
VOLUME /var/lib/relay
ENTRYPOINT ["/usr/local/bin/obs-relay"]
