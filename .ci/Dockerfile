# docker build -t darktable/rawspeed .

# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!! hub.docker.com will not auto-rebuild the image                        !!!
# !!! after making changes here, or if you just want to manually refresh    !!!
# !!! the image, you need to go to:                                         !!!
# https://hub.docker.com/r/darktable/rawspeed/~/settings/automated-builds/  !!!
# !!!                            and press the "Trigger" button.            !!!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

FROM debian:testing
MAINTAINER Roman Lebedev <lebedev.ri@gmail.com>

# needed at least for python-based jsonschema :(
# see https://github.com/Julian/jsonschema/issues/299
# and https://github.com/docker-library/python/issues/13
ENV LANG C.UTF-8
ENV LC_ALL C.UTF-8
ENV LC_MESSAGES C.UTF-8
ENV LANGUAGE C.UTF-8

ENV DEBIAN_FRONTEND noninteractive

# Paper over occasional network flakiness of some mirrors.
RUN echo 'Acquire::Retries "10";' > /etc/apt/apt.conf.d/80retry

# Do not install recommended packages
RUN echo 'APT::Install-Recommends "false";' > /etc/apt/apt.conf.d/80recommends

# Do not install suggested packages
RUN echo 'APT::Install-Suggests "false";' > /etc/apt/apt.conf.d/80suggests

# Assume yes
RUN echo 'APT::Get::Assume-Yes "true";' > /etc/apt/apt.conf.d/80forceyes

# Fix broken packages
RUN echo 'APT::Get::Fix-Missing "true";' > /etc/apt/apt.conf.d/80fixmissin

ENV GCC_VER=9
ENV LLVM_VER=9

# pls keep sorted :)
RUN rm -rf /var/lib/apt/lists/* && apt-get update && \
    apt-get install clang++-$LLVM_VER clang-tidy-$LLVM_VER \
    clang-tools-$LLVM_VER cmake doxygen g++-$GCC_VER git googletest graphviz \
    libc++-$LLVM_VER-dev libjpeg-dev libomp-$LLVM_VER-dev libpugixml-dev \
    libxml2-utils make ninja-build python3-sphinx python3-sphinx-rtd-theme \
    unzip zlib1g-dev && apt-get clean && rm -rf /var/lib/apt/lists/*

# i'd like to explicitly use ld.gold
# while it may be just immeasurably faster, it is known to cause more issues
# than traditional ld.bfd; plus, at this time, ld.gold seems like the future.
RUN dpkg-divert --add --rename --divert /usr/bin/ld.original /usr/bin/ld && \
    ln -s /usr/bin/ld.gold /usr/bin/ld
