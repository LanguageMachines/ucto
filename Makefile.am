ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include m4 config docs tests

EXTRA_DIST = bootstrap.sh AUTHORS TODO NEWS README.md ucto.pc.in ucto-icu.pc.in codemeta.json

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ucto.pc

ChangeLog: NEWS
	git pull; git2cl > ChangeLog

docker:
	docker build -t ucto:latest .

docker-dev:
	docker build -t ucto:dev --build-arg VERSION=development .

deps:
	./build-deps.sh

