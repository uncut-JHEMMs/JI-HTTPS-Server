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
    boost-devel \
    python3-devel \
    lmdb-devel \
    lmdbxx-devel \
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

WORKDIR /dep/catch2
RUN git clone --depth 1 --branch v3.0.0-preview4 https://github.com/catchorg/Catch2.git .
RUN cmake -Bbuild -H. -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr
RUN cmake --build build/ --target install -j

COPY . /docker_build
WORKDIR /docker_build/build
RUN cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_MODULE_PATH=/usr/lib64/cmake/nlohmann_json:/usr/lib64/ ..
RUN cmake --build . --config ${BUILD_TYPE} -j
RUN mkdir /server
RUN cp /docker_build/build/bin/${BUILD_TYPE}/ /server/bin -r
WORKDIR /server

RUN if [ -f /docker_build/cert.pem ]; then cp /docker_build/cert.pem /server/cert.pem; fi
RUN if [ -f /docker_build/key.pem ]; then cp /docker_build/key.pem /server/key.pem; fi
RUN if [ -f /docker_build/config.json ]; then cp /docker_build/config.json /server/config.json; else echo "{}" >> /server/config.json; fi
ENV UTOPIA_CONFIG_FILE=/server/config.json

CMD [ "/server/bin/utopia-server" ]
