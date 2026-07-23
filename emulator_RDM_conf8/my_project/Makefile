all:
	@echo "Compiling all YANG modules in 'yang' directory..."
	@# Only compile files that start with 'module' to avoid compiling submodules directly
	@for f in yang/*.yang; do \
		if grep -q "^module " "$$f"; then \
			echo "Compiling $$f"; \
			confdc -c -o confd-cdb/$$(basename $$f .yang).fxs --yangpath yang $$f; \
		else \
			echo "Skipping submodule $$f"; \
		fi \
	done
	@echo "Generating SSH keys..."
	@ssh-keygen -m PEM -t rsa -f etc/confd/ssh/ssh_host_rsa_key -N "" || true

clean:
	rm -f confd-cdb/*.fxs
