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

ENV GCC_VER=7
ENV LLVM_VER=4.0

# pls keep sorted :)
RUN rm -rf /var/lib/apt/lists/* && apt-get update && \
    apt-get install clang++-$LLVM_VER clang-tidy-$LLVM_VER cmake curl \
    default-jdk-headless default-jre-headless dirmngr doxygen g++-$GCC_VER git \
    gnupg googletest graphviz libjpeg-dev libpugixml-dev libxml2-utils make \
    ninja-build python3-sphinx python3-sphinx-rtd-theme unzip zlib1g-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# sonarqube support.

ENV SONAR_SCANNER_CLI_VERSION=3.0.3.778-linux \
    SONARQUBE_HOME=/opt/sonarqube

RUN set -x \

    # pub   2048R/D26468DE 2015-05-25
    #       Key fingerprint = F118 2E81 C792 9289 21DB  CAB4 CFCA 4A29 D264 68DE
    # uid                  sonarsource_deployer (Sonarsource Deployer) <infra@sonarsource.com>
    # sub   2048R/06855C1D 2015-05-25
    && gpg --keyserver keys.gnupg.net --recv-keys F1182E81C792928921DBCAB4CFCA4A29D26468DE \
    && cd /opt \
    && curl -o sonar-scanner-cli.zip -fSL https://sonarsource.bintray.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$SONAR_SCANNER_CLI_VERSION.zip \
    && curl -o sonar-scanner-cli.zip.asc -fSL https://sonarsource.bintray.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-$SONAR_SCANNER_CLI_VERSION.zip.asc \
    && gpg --batch --verify sonar-scanner-cli.zip.asc sonar-scanner-cli.zip \
    && unzip sonar-scanner-cli.zip \
    && mv sonar-scanner-$SONAR_SCANNER_CLI_VERSION $SONARQUBE_HOME \
    && rm sonar-scanner-cli.zip* \
    && curl -o build-wrapper.zip -fSL https://sonarqube.com/static/cpp/build-wrapper-linux-x86.zip \
    && unzip build-wrapper.zip \
    && mv build-wrapper-linux-x86/* $SONARQUBE_HOME/bin \
    && rm build-wrapper.zip* \
    && rmdir build-wrapper-linux-x86

ENV PATH=$SONARQUBE_HOME/bin:$PATH

# i'd like to explicitly use ld.gold
# while it may be just immeasurably faster, it is known to cause more issues
# than traditional ld.bfd; plus, at this time, ld.gold seems like the future.
RUN dpkg-divert --add --rename --divert /usr/bin/ld.original /usr/bin/ld && \
    ln -s /usr/bin/ld.gold /usr/bin/ld
