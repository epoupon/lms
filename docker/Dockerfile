FROM	alpine:3.10 AS build

WORKDIR	/tmp/workdir

ARG	MAKEFLAGS="-j2"
ARG	FFMPEG_VERSION=4.1.4
ARG	WT_VERSION=4.2.0
ARG	GRAPHICSMAGICK_VERSION=1.3.35
ARG	PSTREAMS_VERSION=1.0.1
ARG	LMS_VERSION=v3.6.3

ARG	PREFIX="/tmp/install"

ARG	BUILD_PACKAGES=" \
	ca-certificates \
	curl \
	bzip2 \
	pkgconfig \
	coreutils \
	autoconf \
	automake \
	libtool \
	g++ \
	make \
	libjpeg-turbo-dev \
	openjpeg-dev \
	jasper-dev \
	libpng-dev \
	tiff-dev \
	nasm \
	yasm \
	curl \
	libogg-dev \
	opus-dev \
	libvorbis-dev \
	lame-dev \
	cmake \
	zlib-dev \
	openssl-dev \
	boost-dev \
	libconfig-dev \
	taglib-dev"

RUN	apk add --no-cache --update ${BUILD_PACKAGES}

## ffmpeg
RUN  \
	DIR=/tmp/ffmpeg && mkdir -p ${DIR} && cd ${DIR} && \
	curl -sLO https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2 && \
	tar -jx --strip-components=1 -f ffmpeg-${FFMPEG_VERSION}.tar.bz2

RUN \
	DIR=/tmp/ffmpeg && mkdir -p ${DIR} && cd ${DIR} && \
	./configure \
	--prefix=${PREFIX} \
	--disable-autodetect \
	--disable-debug \
	--disable-doc \
	--disable-ffplay \
	--disable-ffprobe \
	--disable-openssl \
	--disable-postproc \
	--disable-pixelutils \
	--disable-network \
	--enable-shared \
	--disable-static \
	--enable-gpl \
	--enable-small \
	--enable-version3 \
	--enable-nonfree \
	--enable-libmp3lame \
	--enable-libopenjpeg \
	--enable-libopus \
	--enable-libvorbis \
	--disable-everything \
	--enable-decoder=aac*,ac3*,alac,als,flac,mp3*,libopus,pcm*,libvorbis,wavpack,wma*,libopenjpg,png,tiff \
	--enable-encoder=libmp3lame,libopus,libvorbis \
	--enable-demuxer=aac,aiff,asf,flac,ipod,ogg,matroska,mov,mp3,mp4,wav,wv,webm \
	--enable-muxer=ogg,matroska,mp3,webm \
	--enable-protocol=file,pipe \
	--enable-filter=aresample \
	--extra-libs=-ldl && \
	make && \
	make install && \
	make distclean

# WT
RUN \
	DIR=/tmp/wt && mkdir -p ${DIR} && cd ${DIR} && \
	curl -sLO https://github.com/emweb/wt/archive/${WT_VERSION}.tar.gz && \
	tar -x --strip-components=1 -f ${WT_VERSION}.tar.gz

RUN \
	DIR=/tmp/wt && mkdir -p ${DIR} && cd ${DIR} && \
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX} -DBUILD_EXAMPLES=NO && \
	make && \
	make install

# GraphicsMagick++
RUN \
	DIR=/tmp/graphicsmagick && mkdir -p ${DIR} && cd ${DIR} && \
	curl -sL ftp://ftp.graphicsmagick.org/pub/GraphicsMagick/1.3/GraphicsMagick-${GRAPHICSMAGICK_VERSION}.tar.gz -o ${GRAPHICSMAGICK_VERSION}.tar.gz && \
	tar -x --strip-components=1 -f ${GRAPHICSMAGICK_VERSION}.tar.gz

RUN \
	DIR=/tmp/graphicsmagick && mkdir -p ${DIR} && cd ${DIR} && \
	./configure \
	--prefix=${PREFIX} \
	--enable-shared=yes \
	--enable-static=no \
	--disable-openmp \
	--with-quantum-depth=8 \
	--with-modules=no \
	--with-frozenpaths=no \
	--with-magick-plus-plus=yes \
	--with-perl=no \
	--with-bzlib=no \
	--with-dps=no \
	--with-fpx=no \
	--with-jbig=no \
	--with-webp=no \
	--with-jp2=yes \
	--with-jpeg=yes \
	--with-lcms2=no \
	--with-lzma=no \
	--with-png=yes \
	--with-tiff=yes \
	--with-trio=no \
	--with-ttf=no \
	--with-tcmalloc=no \
	--with-wmf=no \
	--with-xml=no \
	--with-zlib=yes \
	--with-zstd=no \
	--with-x=no && \
	make && \
	make install && \
	make distclean

# libpstreams
RUN \
	DIR=/tmp/libpstreams && mkdir -p ${DIR} && cd ${DIR} && \
	curl -sLO https://sourceforge.net/projects/pstreams/files/pstreams/Release%201.0/pstreams-${PSTREAMS_VERSION}.tar.gz && \
	tar -x --strip-components=1 -f pstreams-${PSTREAMS_VERSION}.tar.gz && \
	prefix=${PREFIX} make install prefix=/ DESTDIR=${PREFIX}

# LMS
RUN \
	DIR=/tmp/lms && mkdir -p ${DIR} && cd ${DIR} && \
	curl -sL https://github.com/epoupon/lms/archive/${LMS_VERSION}.tar.gz -o ${LMS_VERSION}.tar.gz && \
	tar -x --strip-components=1 -f ${LMS_VERSION}.tar.gz

RUN \
	DIR=/tmp/lms && mkdir -p ${DIR} && cd ${DIR} && \
	PKG_CONFIG_PATH=/tmp/install/lib/pkgconfig CXXFLAGS="-I${PREFIX}/include" LDFLAGS="-L${PREFIX}/lib -Wl,--rpath-link=${PREFIX}/lib" cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${PREFIX} && \
	make && \
	LD_LIBRARY_PATH=${PREFIX}/lib make test && \
	make install && \
	mkdir -p ${PREFIX}/etc/ && \
	cp conf/lms.conf ${PREFIX}/etc

# Now copy all the stuff installed in a new folder (/tmp/fakeroot/)
RUN \
	mkdir -p /tmp/fakeroot/bin && \
	for bin in ${PREFIX}/bin/ffmpeg ${PREFIX}/bin/lms*; \
	do \
		strip --strip-all $bin && \
		cp $bin /tmp/fakeroot/bin/; \
	done && \
	for lib in ${PREFIX}/lib/*.so; \
	do \
		strip --strip-all $lib; \
	done && \
	cp -r ${PREFIX}/lib /tmp/fakeroot/lib && \
	cp -r ${PREFIX}/share /tmp/fakeroot/share && \
	rm -rf /tmp/fakeroot/share/doc && \
	rm -rf /tmp/fakeroot/share/man

## Release Stage
FROM		alpine:3.10 AS release
LABEL		maintainer="Emeric Poupon <itmfr@yahoo.fr>"
LABEL		version=${LMS_VERSION}

ARG	RUNTIME_PACKAGES=" \
	openssl \
	libjpeg-turbo \
	openjpeg \
	jasper-libs \
	libpng \
	tiff \
	libogg \
	opus \
	libvorbis \
	lame \
	zlib \
	boost-filesystem \
	boost-program_options \
	boost-system \
	boost-thread \
	libconfig++ \
	taglib"

ARG	LMS_USER=lms
ARG	LMS_GROUP=lms

RUN	apk add --no-cache --update ${RUNTIME_PACKAGES}

RUN	addgroup -S ${LMS_GROUP} && \
	adduser -S -H ${LMS_USER} && \
	adduser ${LMS_USER} ${LMS_GROUP} && \
	mkdir -p /var/lms && chown -R ${LMS_USER}:${LMS_GROUP} /var/lms

VOLUME	/var/lms
VOLUME	/music
VOLUME	/usr/local/etc

USER	${LMS_USER}:${LMS_GROUP}

COPY	--from=build /tmp/fakeroot/ /usr
COPY	--from=build /tmp/fakeroot/share/lms/lms.conf /etc/lms.conf

EXPOSE	5082

ENTRYPOINT	["/usr/bin/lms"]

