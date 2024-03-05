srcdir := ..
reldir-OpenBSD := ${PWD:${srcdir}/%=%}
reldir-Darwin := ${reldir-OpenBSD}
reldir-Linux := ${reldir-OpenBSD}/src
RELDIR=${reldir-${uname}}

prefix?=/usr/local
INSTALL_BINDIR=${prefix}/bin

uname != uname
config	!= cat ${srcdir}/.config | sed -n '/^.*=y$$/p' | sed 's/^\(.*\)=y$$/-D\1/' | tr '\n' ' '
cflags-Darwin	+= -I/usr/local/opt/berkeley-db@4/include
CFLAGS := -g -O0 -Wall -Wno-initializer-overrides ${cflags-${uname}} ${config} \
	-I/usr/local/include -I${srcdir}/include \
	-D_GNU_SOURCE -std=c99 \
	-Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-designator
ldlibs-Linux := -lbsd
LDLIBS := -lxxhash -lpcre -lm -lc -ldb -lndc ${ldlibs-${uname}}
ldflags-Darwin += -L/usr/local/opt/berkeley-db@4/lib
local-ndc-path != realpath ${srcdir}/../ndc
LDFLAGS	:= ${ldflags-${uname}} -L${local-ndc-path} -Wl,-rpath,${local-ndc-path}

exe := ../nd
obj-y := object.o entity.o spacetime.o mcp.o map.o \
	plant.o biome.o noise.o view.o item.o spell.o drink.o \
	mob.o kill.o shop.o create.o look.o match.o \
	move.o player.o utils.o rob.o set.o speech.o wiz.o \
	copyright.o interface.o

src = ${obj-y:%.o=%.c}
LD := gcc

all: ${exe}

$(exe): ${obj-y}
	@echo LD $@
	@${LINK.c} -o $@ $^ ${LDLIBS} -lrt -lutil
	pwd

.c.o:
	@echo CC ${@:%=${RELDIR}/%}
	@${COMPILE.c} -o ${@:%=${RELDIR}/%} ${<:%=${RELDIR}/%}

CTAGS ?= ctags

tags: ${src}
	${CTAGS} --c-types=-m ${src} ../include/*.h

.c.d:
	@echo MKDEP ${@:%=${RELDIR}/%}
	@gcc -MM -o $@ ${CFLAGS} -I../include $<

clean:
	@echo rm ${RELDIR}/{ ${exe} ${obj-y} }
	@rm ${exe} ${obj-y} >/dev/null 2>&1 || true

.PHONY: all clean
.SUFFIXES: .d .o .c

-include ${src:%.c=%.d}
