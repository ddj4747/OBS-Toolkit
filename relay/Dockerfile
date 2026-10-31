ARG RELAY_API_PORT=8080
ARG RELAY_SRTLA_PORT=5000
ARG RELAY_SRT_PORT=8890
ARG RELAY_RTSP_PORT=8554
ARG RELAY_RTP_PORT=8000
ARG RELAY_RTCP_PORT=8001
ARG RELAY_RTMP_PORT=1935
ARG RELAY_WEBRTC_PORT=8889
ARG RELAY_MOQ_PORT=8892
ARG RELAY_MOQ_QUIC_PORT=8893

FROM ubuntu:24.04 AS build

ARG RELAY_API_PORT
ARG RELAY_SRTLA_PORT
ARG RELAY_SRT_PORT
ARG RELAY_RTSP_PORT
ARG RELAY_RTP_PORT
ARG RELAY_RTCP_PORT
ARG RELAY_RTMP_PORT
ARG RELAY_WEBRTC_PORT
ARG RELAY_MOQ_PORT
ARG RELAY_MOQ_QUIC_PORT

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
    -DRELAY_API_PORT="$RELAY_API_PORT" -DRELAY_SRTLA_PORT="$RELAY_SRTLA_PORT" \
    -DRELAY_SRT_PORT="$RELAY_SRT_PORT" -DRELAY_RTSP_PORT="$RELAY_RTSP_PORT" \
    -DRELAY_RTP_PORT="$RELAY_RTP_PORT" -DRELAY_RTCP_PORT="$RELAY_RTCP_PORT" \
    -DRELAY_RTMP_PORT="$RELAY_RTMP_PORT" -DRELAY_WEBRTC_PORT="$RELAY_WEBRTC_PORT" \
    -DRELAY_MOQ_PORT="$RELAY_MOQ_PORT" -DRELAY_MOQ_QUIC_PORT="$RELAY_MOQ_QUIC_PORT" \
    -DCMAKE_TOOLCHAIN_FILE=/deps/generators/conan_toolchain.cmake \
    && cmake --build /build --parallel

FROM ubuntu:24.04

ARG RELAY_API_PORT
ARG RELAY_SRTLA_PORT
ARG RELAY_SRT_PORT
ARG RELAY_RTSP_PORT
ARG RELAY_RTP_PORT
ARG RELAY_RTCP_PORT
ARG RELAY_RTMP_PORT
ARG RELAY_WEBRTC_PORT
ARG RELAY_MOQ_PORT
ARG RELAY_MOQ_QUIC_PORT

EXPOSE ${RELAY_API_PORT}/tcp ${RELAY_SRTLA_PORT}/udp ${RELAY_SRT_PORT}/udp \
       ${RELAY_RTSP_PORT}/tcp ${RELAY_RTSP_PORT}/udp ${RELAY_RTP_PORT}/udp ${RELAY_RTCP_PORT}/udp \
       ${RELAY_RTMP_PORT}/tcp ${RELAY_WEBRTC_PORT}/tcp ${RELAY_MOQ_PORT}/tcp ${RELAY_MOQ_PORT}/udp \
       ${RELAY_MOQ_QUIC_PORT}/udp

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libqt6core6t64 libqt6network6t64 ca-certificates curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home relay \
    && mkdir -p /var/lib/relay && chown relay:relay /var/lib/relay

COPY --from=build /build/obs-relay /build/go-irl /build/mediamtx /usr/local/bin/
USER relay
ENV RELAY_CONFIG_DIR=/var/lib/relay
VOLUME /var/lib/relay
ENTRYPOINT ["/usr/local/bin/obs-relay"]
