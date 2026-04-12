include env/stage00.env

.PHONY: discover-paths check-host report all clean

discover-paths:
	bash scripts/discover_paths.sh

check-host:
	bash scripts/check_host_tools.sh

report:
	bash scripts/generate_stage00_report.sh

all: discover-paths check-host report

clean:
	rm -f output/*.txt output/*.env output/*.md
