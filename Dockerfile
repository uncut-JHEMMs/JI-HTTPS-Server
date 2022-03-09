FROM fedora:35 as base
RUN dnf upgrade -y
RUN dnf install -y \
    git \
    clang \
    make \
    cmake \
    automake \
    libtool \
    ninja-build \
    libmicrohttpd-devel \
    gnutls-devel \
    spdlog-devel \
    procps-ng \
    jq \
    valgrind \
    gnuplot \
    vim \
    iproute

ENV CC=clang
ENV CXX=clang++
ENV BUILD_TYPE=Release

FROM base as build
WORKDIR /dep/libhttpserver
RUN git clone https://github.com/etr/libhttpserver.git .
RUN git reset 4eb69fb --hard
RUN ./bootstrap
WORKDIR build
RUN ../configure --prefix=/usr
RUN make -j
RUN make install

WORKDIR /dep/json
RUN git clone --depth 1 --branch v3.10.5 https://github.com/nlohmann/json.git .
WORKDIR build
RUN cmake -G Ninja -DJSON_BuildTests=OFF -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
RUN cmake --install . --config ${BUILD_TYPE} --prefix /usr

COPY . /docker_build
WORKDIR /docker_build/build
RUN cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_MODULE_PATH=/usr/lib64/cmake/nlohmann_json:/usr/lib64/ ..
RUN cmake --build . --config ${BUILD_TYPE} -j
RUN mkdir /server
RUN cp /docker_build/build/bin/${BUILD_TYPE}/ /server/bin -r
WORKDIR /server
RUN cp /docker_build/cert.pem /server/cert.pem
RUN cp /docker_build/key.pem /server/key.pem
RUN cp /docker_build/config.json /server/config.json
ENV UTOPIA_CONFIG_FILE=/server/config.json

CMD [ "/server/bin/HTTPS-Server", "-C", "/server/cert.pem", "-K", "/server/key.pem" ]
