PDF := main.pdf
TEX := main.tex

LATEXMK_FLAGS := -pdf -shell-escape -interaction=nonstopmode -halt-on-error -file-line-error

.PHONY: all watch clean distclean

all: $(PDF)

$(PDF): $(TEX) preamble.tex biblio.bib $(wildcard chapters/*.tex) $(wildcard figures/*.tex)
	latexmk $(LATEXMK_FLAGS) $(TEX)

watch:
	latexmk $(LATEXMK_FLAGS) -pvc $(TEX)

clean:
	latexmk -C
	rm -rf _minted-* *.bbl *.run.xml *.bcf *.synctex.gz

distclean: clean
	rm -f $(PDF)
