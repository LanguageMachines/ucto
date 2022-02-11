#TODO: revert channel to 'latest' instead of 'edge' once 3.16 is released
FROM alpine:edge
#VERSION can be "stable" or "development"
ARG VERSION="stable"
LABEL org.opencontainers.image.authors="Maarten van Gompel <proycon@anaproy.nl>"
LABEL description="Ucto, rule-based tokenizer"

RUN mkdir -p /data
RUN mkdir -p /usr/src/ucto
COPY . /usr/src/ucto

RUN if [ "$VERSION" = "stable" ]; then \
        rm -Rf /usr/src/ucto &&\
        echo -e "----------------------------------------------------------\nNOTE: Installing latest stable release as provided by Alpine package manager.\nThis version may diverge from the one in the git master tree!\nFor development, build with --build-arg VERSION=development.\n----------------------------------------------------------\n" &&\
        apk update && apk add ucto; \
    else \
        echo -e "----------------------------------------------------------\nNOTE: Building development versions from source.\nThis version may be experimental and contains bugs!\nFor production, build with --build-arg VERSION=stable ----------------------------------------------------------\n" &&\
        apk add build-base autoconf-archive autoconf automake libtool libtar-dev libbz2 bzip2-dev icu-dev libxml2-dev libexttextcat-dev git &&\
        cd /usr/src/ &&\
        git clone https://github.com/LanguageMachines/ticcutils && cd ticcutils && sh ./bootstrap.sh && ./configure && make && make install && cd .. &&\
        git clone https://github.com/LanguageMachines/libfolia && cd libfolia && sh ./bootstrap.sh && ./configure && make && make install && cd .. &&\
        git clone https://github.com/LanguageMachines/uctodata && cd uctodata && sh ./bootstrap.sh && ./configure && make && make install && cd .. &&\
        cd ucto && sh ./bootstrap.sh && ./configure && make && make install; \
    fi

WORKDIR /

ENTRYPOINT [ "ucto" ]
