SOURCE = prepinfo.twjr
TEXISOURCE = prepinfo.texi

all: prepinfo.awk prepinfo.pdf

$(TEXISOURCE): $(SOURCE)
	./aux/jrweave $(SOURCE) > $(TEXISOURCE)

prepinfo.awk: $(SOURCE)
	./aux/jrtangle $(SOURCE)

prepinfo.pdf: $(TEXISOURCE)
	texi2dvi --pdf --batch --build-dir=prepinfo.t2p -o $@ $(TEXISOURCE)

html: prepinfo.html

prepinfo.html: $(TEXISOURCE)
	makeinfo --no-split --html $(TEXISOURCE)

spell:
	export LC_ALL=C; \
	spell $(SOURCE) | sort -u | comm -23 - aux/wordlist

clean:
	for i in awk pdf html t2p texi ; \
	do $(RM) -fr prepinfo.$$i ; \
	done
