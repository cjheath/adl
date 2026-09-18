#
# Analysis of the ADL parser: code size, maximum stack, dynamic memory, and how
# much the Store actually shares its strings.
#
# The measurements are taken across the revisions that produced the current
# tree, each assembled from git, so the whole comparison can be re-run from a
# clean checkout. Everything generated lands in build/ and out/.
#
#   make            measure everything and print the tables
#   make summary    print the tables from the last run, without re-measuring
#   make clean      remove build/ and out/
#
PYTHON	?=	python3

all:
	@$(PYTHON) scripts/analyse.py

summary:
	@cat out/tables.txt 2>/dev/null || { echo "no tables yet - run make first"; exit 1; }

clean:
	rm -rf build out

.PHONY:	all summary clean
